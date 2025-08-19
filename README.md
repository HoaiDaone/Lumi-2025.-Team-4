# Lumi-2025.-Team-4
**ESP32 4-Button Chat Interface with ST7735 Display**

The system is built around an ESP32 microcontroller connected to a 1.8″ 128×160 pixel TFT display (ST7735 driver) over SPI. The Adafruit_ST7735 and Adafruit_GFX libraries are used to render graphics and text on the display. SPI communication requires a few pins (chip-select, data/command, reset, MOSI, SCLK) between the ESP32 and the display. Five pushbuttons (UP, DOWN, OK, CANCEL, and SEND) serve as the user interface, allowing menu navigation, character selection, and message sending. A piezo buzzer provides audible feedback on button presses.

**Menu and UI Navigation**

The program implements a hierarchical menu and screen system controlled by the four main buttons.
At the Main Menu, the user can choose between Mail and Help. Navigating is done with UP/DOWN (to
change the selection) and OK (to confirm) or CANCEL (to go back). From Mail, the program shows an
inbox of known device IDs; selecting one opens the Conversation view with that contact. From the
Conversation view, the user can press OK to enter Write Message mode or CANCEL to return. The Help
screen displays instructions. Pressing UP at the top of the inbox list enters Add Device mode, where a new device ID can be entered.

- Main Menu: Displays two options “Mail” and “Help”. UP/DOWN moves the highlight, OK selects.
- Mail List (Inbox): Shows stored device IDs (contacts). The current selection is highlighted. One entry (“0: Add New ID”) allows adding a new contact by entering its ID.
- Conversation View: Lists the messages in the selected conversation. Outgoing messages are prefixed with “You:” and right-aligned; incoming messages are left-aligned. OK enters message writing mode.
- Write Message Screen: Shows the recipient ID and the message buffer. A 4×N on-screen “QWERTY” keyboard is displayed; the current character is highlighted. UP/DOWN scrolls through characters, OK appends the character to the buffer, and a long-press or the SEND button sends the message.
- Add Device Screen: Similar to Write Message, but the buffer collects a new device ID. Once entered, the SEND button checks the server and adds the device if valid.
- Help Screen: Static instructions on button usage (e.g. “UP: prev char, DOWN: next char, OK: select/add, CANCEL: back/delete”).

Each screen is drawn using ` tft.setCursor() ` , ` tft.print() ` , and fill/draw routines. The AdafruitGFX library makes it easy to print text and draw simple shapes on the ST7735 display . The codeupdates the display on every state change or input event to reflect the current menu.


**Messaging Data and Memory**

The device maintains an inbox of up to 6 contacts. Device IDs are stored in a `String mail_ids[MAX_MAILS]` array, and each has an associated Conversation struct holding up to 12
messages ( `msgs[]` ). The program’s own ID ( `my_device_id` , default "01") and a human-readable
name are fixed. When a new ID is added, it is verified against the server before appending to
`mail_ids` and creating an empty conversation.

Key data structures include: 

- `mail_ids` : array of contact IDs (strings) in the inbox.
- `convs` : parallel array of Conversation objects, each storing an array of past messages (`msgs[]` ).
- `write_buf` : character buffer for composing an outgoing message.
- `new_id_buf` : buffer for typing a new device ID. 
    
When sending or receiving messages, the code calls `pushMessageToConv(index, message)` , which
appends the message to the conversation (shifting older messages out if full). For example, after
sending a message “Hello”, the code does `pushMessageToConv(mail_index, "You: "+payload)`
to add it to the current conversation. An empty-string message is also used to initialize a conversation on both devices ( `createEmptyConversation` ).

**Network Communication and JSON Processing**

Messages and device registration are sent through an HTTP server (default at `http://
192.168.1.15:5000` ). The code uses `HTTPClient` (built into Arduino/ESP32) for all network
requests. Typical usage is:

```bash
HTTPClient http;
http.begin(url);
http.addHeader("Content-Type", "application/json");
int code = http.POST(jsonPayload);
```

This pattern is used for sending JSON data . For example, to register the device the code POSTs to
`SERVER_BASE_URL/devices/register` with payload
`{"device_id":"01","name":"esp32_01"}` . To send a chat message it POSTs to
`/messages/send` with
`{"sender_id":"<from>","receiver_id":"<to>","content":"<text>"}` .

Receiving messages involves HTTP GET requests. The function `fetchInboxFromServer(deviceId)`
performs a GET on `/messages/inbox?device_id=<id>` . A successful response (HTTP 200) returns a
JSON array of message objects, each with `sender_id` and `content` . The code reads this into a
`String payload` and uses the ArduinoJson library to parse it. For example, it does:

```bash
DynamicJsonDocument doc(2048);
DeserializationError error = deserializeJson(doc, payload);
JsonArray array = doc.as<JsonArray>();
for (JsonObject msg : array) {
    String sender = msg["sender_id"];
    String content = msg["content"];
    // ...
}
```

This follows the typical pattern of creating a DynamicJsonDocument and calling
`deserializeJson(doc, jsonString)` . The code then iterates the resulting `JsonArray` to
extract each message.
If `checkDeviceIdExists(newId)` is called, it sends a GET to `/devices/?id=<newId>` and checks
if the response contains the ID. All server interactions assume the ESP32 is connected to Wi-Fi (the code attempts up to 15 seconds to connect in `setup()` ).

**Finite State Machine and Input Handling**

The program logic is organized as a finite state machine. An `enum AppState { STATE_MAIN_MENU,
STATE_MAIL_LIST, STATE_MAIL_CONV, STATE_MAIL_WRITE, STATE_HELP, STATE_ADD_ID };`
defines the possible screens. The global variable `state` holds the current state. Using an enum for states provides clear, human-readable names . In the main loop, input events set flags or change states, and the code responds based on the current `state` .

An example of this pattern is: 

```bash
switch(state) {
    case STATE_MAIN_MENU:
        // handle menu navigation
        break;
    case STATE_MAIL_LIST:
        // handle inbox navigation
        break;
    // ... etc.
}
```

This follows the recommended approach of using `if` / `switch` on the state variable to perform statespecific actions . For instance, in `handleStateButtons()` , a button press may change
`main_index` when in `STATE_MAIN_MENU` or append a character to `write_buf` when in
`STATE_MAIL_WRITE` . Debouncing is managed by a simple timestamp check ( `lastDebounce` ) to
ignore rapid repeats.


Key points of the state machine implementation: 
- Enums for States: The code declares named states
(e.g. `STATE_MAIL_LIST` ) and a variable to hold the current state .
- Switch-Case Logic: In each
loop iteration, a `switch(state)` chooses behavior, mirroring typical Arduino FSM examples . -
Transitions: State changes occur on button events (e.g. OK or CANCEL), and certain actions (like
sending a message) trigger a state return (back to conversation view).
- Debounce and Long-Press: A
simple delay-based debounce ( `lastDebounce` ) avoids repeated triggers. A separate long-press
detection on the OK button allows sending by holding OK while writing a message (similar in effect to using the SEND button).

Overall, this structure cleanly separates the UI screens and input handling. The device loops
continuously, checking buttons and network periodically, updating the display on any state or data
change.

**Key Features and Flow**

- Menu Navigation: The four-button interface cycles through menus and text entry without a touch screen. Controls are consistent: UP/DOWN move highlight or selection; OK confirms or adds characters; CANCEL goes back or deletes.
- On-Screen Keyboard: A 4-row keyboard layout is drawn on-screen. The code computes a global index for each character and highlights the selected one, similar to cursor navigation on a character grid.
- Bi-Directional Messaging: Outgoing messages are sent via HTTP POST and locally stored; incoming messages are periodically fetched (every 5 seconds) via HTTP GET and parsed with `ArduinoJson`.
- Device Registration: On startup the ESP32 registers itself with the server (`/devices/register`). Users can also add other device IDs (if they exist on the server), enabling new chat contacts dynamically.
- Help Screen: Provides usage instructions so the user knows the function of each button.

Sources: This system leverages standard Arduino libraries and practices. For example, using `HTTPClient` for GET/POST requests (as shown in example code ) and `ArduinoJson` for parsing JSON responses are common techniques. Using an `enum` and `switch` for state machines is a recommended approach in embedded UI design .

---

1.  **GitHub - adafruit/Adafruit-ST7735-Library:** This is a library for the Adafruit 1.8" SPI display `http://www.adafruit.com/products/358` and `http://www.adafruit.com/products/618`
    `https://github.com/adafruit/Adafruit-ST7735-Library`

2.  **Interface TFT ST7735 Display with ESP32**
    `https://www.makerguides.com/interface-tft-st7735-display-with-esp32/`

3.  **ESP32 HTTP GET and HTTP POST with Arduino IDE | Random Nerd Tutorials**
    `https://randomnerdtutorials.com/esp32-http-get-post-arduino/`

4.  **DynamicJsonDocument | ArduinoJson 6**
    `https://arduinojson.org/v6/api/dynamicjsondocument/`

5.  **How To Program an Arduino Finite State Machine**
    `https://www.digikey.com/en/maker/tutorials/2023/how-to-program-an-arduino-finite-state-machine`





