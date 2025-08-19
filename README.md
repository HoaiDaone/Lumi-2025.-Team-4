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

    . Main Menu: Displays two options “Mail” and “Help”. UP/DOWN moves the highlight, OK selects.
  
    . Mail List (Inbox): Shows stored device IDs (contacts). The current selection is highlighted. One entry (“0: Add New ID”) allows adding a new contact by entering its ID.

    . Conversation View: Lists the messages in the selected conversation. Outgoing messages are prefixed with “You:” and right-aligned; incoming messages are left-aligned. OK enters message writing mode.

    . Write Message Screen: Shows the recipient ID and the message buffer. A 4×N on-screen “QWERTY” keyboard is displayed; the current character is highlighted. UP/DOWN scrolls through characters, OK appends the character to the buffer, and a long-press or the SEND button sends the message.

    . Add Device Screen: Similar to Write Message, but the buffer collects a new device ID. Once entered, the SEND button checks the server and adds the device if valid.

    . Help Screen: Static instructions on button usage (e.g. “UP: prev char, DOWN: next char, OK: select/add, CANCEL: back/delete”).

Each screen is drawn using ` tft.setCursor() ` , ` tft.print() ` , and fill/draw routines. The AdafruitGFX library makes it easy to print text and draw simple shapes on the ST7735 display . The codeupdates the display on every state change or input event to reflect the current menu.


**Messaging Data and Memory**

