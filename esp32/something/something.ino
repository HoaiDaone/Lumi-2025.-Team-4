// ESP32 Messenger UI (4-buttons) + ST7735
// - 4 nút: UP, DOWN, OK, CANCEL
// - Menus: Main -> Mail / Help
// - Mail: danh sách mail, xem hội thoại, soạn tin (bằng nút)
// - Help: hướng dẫn

#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <ArduinoJson.h>

// --- Server URLs (quản lý ở đây) ---
const char* SERVER_BASE_URL = "http://192.168.1.15:5000";
const char* REGISTER_DEVICE_ENDPOINT = "/devices/register";
const char* SEND_MESSAGE_ENDPOINT = "/messages/send";
const char* FETCH_INBOX_ENDPOINT = "/messages/inbox";

// --- PIN cấu hình (thay đổi nếu cần) ---
#define TFT_CS     11
#define TFT_DC     8
#define TFT_RST    18
#define TFT_MOSI   9
#define TFT_SCLK   10
#define TFT_LED    17
#define BUZZER_PIN 35

// 4 nút
#define BTN_UP     16   // di chuyển lên / ký tự trước
#define BTN_DOWN   15   // di chuyển xuống / ký tự sau
#define BTN_OK     7    // chọn / xác nhận
#define BTN_CANCEL 6    // quay lại / xóa
#define BTN_SEND   0    // gửi
// Mạng (mặc định) - sử dụng để kết nối tự động
const char* default_ssid = "My Home";
const char* default_password = "0898624555";

// ST7735 (HSPI)
SPIClass hspi(HSPI);
Adafruit_ST7735 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);

// --- Hệ thống trạng thái ---
enum AppState {
  STATE_MAIN_MENU,
  STATE_MAIL_LIST,
  STATE_MAIL_CONV,
  STATE_MAIL_WRITE,
  STATE_HELP,
  STATE_ADD_ID
};
AppState state = STATE_MAIN_MENU;

// Các biến điều hướng menu
int main_index = 0;            // 0: Mail, 1: Help
int mail_index = 0;

// Biến nhập ID mới cho inbox
char new_id_buf[64] = "";
int new_id_len = 0;

// Dữ liệu mail / conversation
#define MAX_MAILS 6
#define MAX_MESSAGES_PER_1 12
String mail_ids[MAX_MAILS];
int mail_count = 0;

String my_device_id = "Device01";
String name_device = "esp32_01";

struct Conversation {
  String msgs[MAX_MESSAGES_PER_1];
  int size;
};
Conversation convs[MAX_MAILS];

// Soạn tin: buffer
char write_buf[128];
int write_len = 0;

// --- BẢNG BÀN PHÍM QWERTY 4 HÀNG (được yêu cầu) ---
// Hàng: "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM_<", "1234567890"
// Lưu ý: ký tự '<' vẫn là một ký tự bình thường (không dùng để xóa).
const char* keyboard_rows[] = {
   "QWERTYUIOP:;",
  "ASDFGHJKL()_/",
  "ZXCVBNM.,!?",
  "1234567890"
};
const int KB_ROWS = sizeof(keyboard_rows) / sizeof(keyboard_rows[0]);
int kb_row_lengths[KB_ROWS];
int KB_TOTAL = 0;             // tổng số phím
int char_index = 0;           // chỉ số toàn cục (0..KB_TOTAL-1) cho ký tự chọn

// Debounce
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 200;

// helper beep
void beep(int ms = 80) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// --- Helper keyboard utilities ---
void initKeyboard() {
  KB_TOTAL = 0;
  for (int r = 0; r < KB_ROWS; r++) {
    int len = strlen(keyboard_rows[r]);
    kb_row_lengths[r] = len;
    KB_TOTAL += len;
  }
  if (char_index >= KB_TOTAL) char_index = 0;
}

char getKeyAtIndex(int idx) {
  int acc = 0;
  for (int r = 0; r < KB_ROWS; r++) {
    int len = kb_row_lengths[r];
    if (idx < acc + len) {
      return keyboard_rows[r][idx - acc];
    }
    acc += len;
  }
  return '?';
}

// --- Hàm vẽ giao diện ---
void drawHeader(const String &title) {
  tft.fillRect(0, 0, 160, 16, ST77XX_BLACK);
  tft.setCursor(2, 1);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(title);
}

// Main menu
void drawMainMenu() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("Main Menu");

  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);

  tft.setCursor(8, 26);
  tft.print((main_index == 0) ? "> Mail" : "  Mail");

  tft.setCursor(8, 46);
  tft.print((main_index == 1) ? "> Help" : "  Help");
}

// Mail list
void drawMailList() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("Mail - Inbox");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  int display_max = 4;
  int start = mail_index - 1; // center selection if possible
  if (start < 0) start = 0;
  if (start > mail_count - display_max) start = max(0, mail_count - display_max);

  for (int i = 0; i < display_max && (start + i) < mail_count; i++) {
    int idx = start + i;
    tft.setCursor(8, 20 + i*18);
    if (idx == mail_index) {
      tft.setTextColor(ST77XX_WHITE);
      tft.fillRect(4, 18 + i*18 - 2, 152, 16, ST77XX_BLUE);
      tft.setCursor(8, 20 + i*18);
      tft.print(mail_ids[idx]);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.print(mail_ids[idx]);
    }
  }

  tft.setCursor(8, 110);
  tft.setTextColor(ST77XX_RED);
  tft.print("OK: Open  CANCEL: Back");
}

// Conversation view
void drawConversation() {
  tft.fillScreen(ST77XX_WHITE);
  String title = "Chat: " + mail_ids[mail_index];
  drawHeader(title);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  int y = 18;
  int start = max(0, convs[mail_index].size - 6); // show last up to 6
  for (int i = start; i < convs[mail_index].size; i++) {
    String m = convs[mail_index].msgs[i];
    tft.setCursor(6, y);
    if (m.startsWith("You: ")) {
      int w = m.length() * 6;
      int x = 160 - w - 6;
      tft.setCursor(x, y);
      tft.print(m);
    } else {
      tft.print(m);
    }
    y += 12;
    if (y > 100) break;
  }

  tft.setCursor(6, 110);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("OK: Write  CANCEL: Back");
}

// Write message screen (QWERTY grid)
void drawWriteScreen() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("Write Message");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  tft.setCursor(6, 22);
  tft.print("To: ");
  tft.print(mail_ids[mail_index]);

  tft.setCursor(6, 36);
  tft.print("Msg: ");
  // print buffer (wrap if long)
  tft.setCursor(6, 48);
  tft.setTextColor(ST77XX_BLACK);
  tft.print(write_buf);

  // Draw QWERTY grid
  tft.setTextSize(1);
  int cell_w = 12; // cell pixel width
  int cell_h = 12;
  int top_y = 68;
  for (int r = 0; r < KB_ROWS; r++) {
    int row_len = kb_row_lengths[r];
    int row_width = row_len * cell_w;
    int start_x = (160 - row_width) / 2;
    for (int c = 0; c < row_len; c++) {
      int idx = 0;
      // compute global index for (r,c)
      int acc = 0;
      for (int rr = 0; rr < r; rr++) acc += kb_row_lengths[rr];
      idx = acc + c;

      int x = start_x + c * cell_w;
      int y = top_y + r * (cell_h + 2);

      if (idx == char_index) {
        // highlight selected key
        tft.fillRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLUE);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
        tft.setTextColor(ST77XX_BLACK);
      } else {
        // normal key
        tft.drawRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLACK);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
      }
    }
  }

}

// Add ID screen (similar to write)
void drawAddIdScreen() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("Add New Device");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  tft.setCursor(6, 36);
  tft.print("ID: ");
  // print buffer
  tft.setCursor(6, 48);
  tft.print(new_id_buf);

  // Draw QWERTY grid
  tft.setTextSize(1);
  int cell_w = 12; // cell pixel width
  int cell_h = 12;
  int top_y = 68;
  for (int r = 0; r < KB_ROWS; r++) {
    int row_len = kb_row_lengths[r];
    int row_width = row_len * cell_w;
    int start_x = (160 - row_width) / 2;
    for (int c = 0; c < row_len; c++) {
      int idx = 0;
      // compute global index for (r,c)
      int acc = 0;
      for (int rr = 0; rr < r; rr++) acc += kb_row_lengths[rr];
      idx = acc + c;

      int x = start_x + c * cell_w;
      int y = top_y + r * (cell_h + 2);

      if (idx == char_index) {
        // highlight selected key
        tft.fillRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLUE);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
        tft.setTextColor(ST77XX_BLACK);
      } else {
        // normal key
        tft.drawRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLACK);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
      }
    }
  }

  tft.setCursor(6, 110);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("SEND: Confirm  CANCEL: Back");
}

// Help screen
void drawHelp() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("Help");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  tft.setCursor(6, 22);
  tft.println("4 button control:");
  tft.setCursor(6, 40);
  tft.println("UP: prev char");
  tft.setCursor(6, 56);
  tft.println("DOWN: next char");
  tft.setCursor(6, 72);
  tft.println("OK: select / add");
  tft.setCursor(6, 88);
  tft.println("CANCEL: back / delete");
  tft.setCursor(6, 110);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("CANCEL: Back");
}

// Khai báo hàm checkDeviceIdExists ở đầu file
bool checkDeviceIdExists(const String &id);

// Khai báo hàm checkSendButton ở đầu file
void checkSendButton();

// --- Input (button) helpers ---
bool pressedUp() {
  return digitalRead(BTN_UP) == LOW;
}
bool pressedDown() {
  return digitalRead(BTN_DOWN) == LOW;
}
bool pressedOk() {
  return digitalRead(BTN_OK) == LOW;
}
bool pressedCancel() {
  return digitalRead(BTN_CANCEL) == LOW;
}

// Append a message to conversation (for the selected mail)
void pushMessageToConv(int mailIdx, const String &m) {
  Conversation &c = convs[mailIdx];
  if (c.size < MAX_MESSAGES_PER_1) {
    c.msgs[c.size++] = m;
  } else {
    // shift
    for (int i = 1; i < MAX_MESSAGES_PER_1; i++) c.msgs[i-1] = c.msgs[i];
    c.msgs[MAX_MESSAGES_PER_1-1] = m;
  }
}

// Thêm các hàm tích hợp server:

// Đăng ký thiết bị mới lên server
void registerDeviceToServer(const String &deviceId, const String &name) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(SERVER_BASE_URL) + REGISTER_DEVICE_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"device_id\":\"" + deviceId + "\",\"name\":\"" + name + "\"}";
    int httpResponseCode = http.POST(jsonData);
    Serial.print("Dang ky thiet bi: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode <= 0) {
      Serial.println("Loi: Khong ket noi duoc server hoac server khong phan hoi!");
    }
    http.end();
  } else {
    Serial.println("Loi: WiFi chua ket noi!");
  }
}

// Gửi tin nhắn lên server
void sendMessageToServer(const String &fromId, const String &toId, const String &content) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(SERVER_BASE_URL) + SEND_MESSAGE_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"sender_id\":\"" + fromId + "\",\"receiver_id\":\"" + toId + "\",\"content\":\"" + content + "\"}";
    int httpResponseCode = http.POST(jsonData);
    Serial.print("Da gui: ");
    Serial.println(content);
    http.end();
  }
}

// Nhận tin nhắn từ server (inbox)
void fetchInboxFromServer(const String &deviceId) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(SERVER_BASE_URL) + String(FETCH_INBOX_ENDPOINT) + "?device_id=" + deviceId;
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      Serial.println("Tin nhan moi:");
      Serial.println(payload);
      DynamicJsonDocument doc(2048); // Adjust size as needed
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      JsonArray array = doc.as<JsonArray>();
      for (JsonObject msg : array) {
        String sender = msg["sender_id"];
        String content = msg["content"];
        int idx = -1;
        for (int i = 0; i < mail_count; i++) {
          if (mail_ids[i] == sender) {
            idx = i;
            break;
          }
        }
        if (idx == -1) {
          if (mail_count < MAX_MAILS) {
            mail_ids[mail_count] = sender;
            convs[mail_count].size = 0;
            idx = mail_count;
            mail_count++;
          } else {
            continue; // Inbox full
          }
        }
        pushMessageToConv(idx, content);
      }
    }
    http.end();
  }
}

// Định nghĩa hàm checkDeviceIdExists
bool checkDeviceIdExists(const String &id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(SERVER_BASE_URL) + "/devices/?id=" + id;
    http.begin(url);
    int httpResponseCode = http.GET();
    bool exists = false;
    if (httpResponseCode == 200) {
      String payload = http.getString();
      exists = (payload.indexOf(id) >= 0);
    }
    http.end();
    return exists;
  }
  return false;
}

// Định nghĩa hàm checkSendButton nếu chưa có
void checkSendButton() {
  static bool sendWasDown = false;
  if (digitalRead(BTN_SEND) == LOW) {
    if (!sendWasDown) {
      sendWasDown = true;
      if (state == STATE_MAIL_WRITE && write_len > 0) {
        sendMessageNetwork(write_buf, mail_ids[mail_index]);
        write_len = 0;
        write_buf[0] = '\0';
        state = STATE_MAIL_CONV;
        drawConversation();
        beep(100);
      } else if (state == STATE_ADD_ID && new_id_len > 0) {
        String id = String(new_id_buf);
        if (checkDeviceIdExists(id)) {
          if (mail_count < MAX_MAILS) {
            mail_ids[mail_count] = id;
            convs[mail_count].size = 0;
            mail_count++;
            mail_index = mail_count - 1;
          }
          new_id_len = 0;
          new_id_buf[0] = '\0';
          state = STATE_MAIL_LIST;
          drawMailList();
          beep(100);
        } else {
          tft.fillRect(0, 110, 160, 18, ST77XX_WHITE);
          tft.setCursor(6, 110);
          tft.setTextColor(ST77XX_RED);
          tft.print("ID not found!");
          delay(2000);
          drawAddIdScreen();
        }
      }
    }
  } else {
    sendWasDown = false;
  }
}

// --- Setup & loop ---
void setupPins() {
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(TFT_LED, OUTPUT); digitalWrite(TFT_LED, HIGH);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);
  pinMode(BTN_SEND, INPUT_PULLUP);
}

void setup() {
  Serial.begin(115200);
  setupPins();

  // init tft
  hspi.begin(TFT_SCLK, -1, TFT_MOSI);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_WHITE);

  // init keyboard info
  initKeyboard();

  // init conversations empty
  for (int i = 0; i < MAX_MAILS; i++) convs[i].size = 0;

  // Kết nối WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(default_ssid, default_password);
  int wifiTries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTries < 30) {
    delay(500);
    Serial.print(".");
    wifiTries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    // Đăng ký thiết bị mới với Server (dùng deviceId đầu tiên)
    Serial.print("Dang ky thiet bi len server...");
    registerDeviceToServer(my_device_id, name_device);
  } else {
    Serial.println("\nWiFi connection FAILED!");
  }

  drawMainMenu();
}

// Thay thế gửi tin nhắn demo bằng gửi lên server
void sendMessageNetwork(const char *payload, const String &toId) {
  sendMessageToServer(my_device_id, toId, String(payload));
  pushMessageToConv(mail_index, String("You: ") + String(payload));
}

// Có thể gọi fetchInboxFromServer(my_device_id) khi cần cập nhật tin nhắn mới

// --- State machine variables for repeating press handling
void handleStateButtons() {
  if (millis() - lastDebounce < DEBOUNCE_MS) return;

  if (pressedUp()) {
    lastDebounce = millis();
    beep(30);
    switch (state) {
      case STATE_MAIN_MENU:
        main_index = (main_index - 1 + 2) % 2;
        drawMainMenu();
        break;
      case STATE_MAIL_LIST:
        if (mail_index == 0) {
          state = STATE_ADD_ID;
          new_id_len = 0;
          new_id_buf[0] = '\0';
          char_index = 0;
          drawAddIdScreen();
        } else {
          mail_index = max(0, mail_index - 1);
          drawMailList();
        }
        break;
      case STATE_MAIL_CONV:
        // no-op
        break;
      case STATE_MAIL_WRITE:
        // previous key
        if (KB_TOTAL > 0) {
          char_index = (char_index - 1 + KB_TOTAL) % KB_TOTAL;
        }
        drawWriteScreen();
        break;
      case STATE_HELP:
        break;
      case STATE_ADD_ID:
        if (KB_TOTAL > 0) {
          char_index = (char_index - 1 + KB_TOTAL) % KB_TOTAL;
        }
        drawAddIdScreen();
        break;
    }
  } else if (pressedDown()) {
    lastDebounce = millis();
    beep(30);
    switch (state) {
      case STATE_MAIN_MENU:
        main_index = (main_index + 1) % 2;
        drawMainMenu();
        break;
      case STATE_MAIL_LIST:
        mail_index = min(mail_count - 1, mail_index + 1);
        drawMailList();
        break;
      case STATE_MAIL_CONV:
        break;
      case STATE_MAIL_WRITE:
        if (KB_TOTAL > 0) {
          char_index = (char_index + 1) % KB_TOTAL;
        }
        drawWriteScreen();
        break;
      case STATE_HELP:
        break;
      case STATE_ADD_ID:
        if (KB_TOTAL > 0) {
          char_index = (char_index + 1) % KB_TOTAL;
        }
        drawAddIdScreen();
        break;
    }
  } else if (pressedOk()) {
    lastDebounce = millis();
    beep(50);
    switch (state) {
      case STATE_MAIN_MENU:
        if (main_index == 0) {
          state = STATE_MAIL_LIST;
          if (mail_index >= mail_count) mail_index = 0;
          drawMailList();
        } else {
          state = STATE_HELP;
          drawHelp();
        }
        break;
      case STATE_MAIL_LIST:
        if (mail_count > 0) {
          state = STATE_MAIL_CONV;
          drawConversation();
        }
        break;
      case STATE_MAIL_CONV:
        state = STATE_MAIL_WRITE;
        write_len = 0;
        write_buf[0] = '\0';
        char_index = 0;
        drawWriteScreen();
        break;
      case STATE_MAIL_WRITE:
        // Add selected char to buffer
        if (write_len < (int)sizeof(write_buf) - 2 && KB_TOTAL > 0) {
          write_buf[write_len++] = getKeyAtIndex(char_index);
          write_buf[write_len] = '\0';
        }
        drawWriteScreen();
        break;
      case STATE_HELP:
        break;
      case STATE_ADD_ID:
        // Add selected char to buffer
        if (new_id_len < (int)sizeof(new_id_buf) - 2 && KB_TOTAL > 0) {
          new_id_buf[new_id_len++] = getKeyAtIndex(char_index);
          new_id_buf[new_id_len] = '\0';
        }
        drawAddIdScreen();
        break;
    }
  } else if (pressedCancel()) {
    lastDebounce = millis();
    beep(40);
    switch (state) {
      case STATE_MAIN_MENU:
        // nothing
        break;
      case STATE_MAIL_LIST:
        state = STATE_MAIN_MENU;
        drawMainMenu();
        break;
      case STATE_MAIL_CONV:
        state = STATE_MAIL_LIST;
        drawMailList();
        break;
      case STATE_MAIL_WRITE:
        // delete last char; if empty -> back to conv
        if (write_len > 0) {
          write_buf[--write_len] = '\0';
          drawWriteScreen();
        } else {
          state = STATE_MAIL_CONV;
          drawConversation();
        }
        break;
      case STATE_HELP:
        state = STATE_MAIN_MENU;
        drawMainMenu();
        break;
      case STATE_ADD_ID:
        // delete last char; if empty -> back to list
        if (new_id_len > 0) {
          new_id_buf[--new_id_len] = '\0';
          drawAddIdScreen();
        } else {
          state = STATE_MAIL_LIST;
          drawMailList();
        }
        break;
    }
  }
}

// Long-press OK for sending message (kept as before, removed WiFi part)
void checkLongPressSend() {
  static unsigned long okDownTime = 0;
  static bool okWasDown = false;

  if (digitalRead(BTN_OK) == LOW) {
    if (!okWasDown) {
      okWasDown = true;
      okDownTime = millis();
    } else {
      if (millis() - okDownTime > 700) {
        if (state == STATE_MAIL_WRITE && write_len > 0) {
          sendMessageNetwork(write_buf, mail_ids[mail_index]);
          write_len = 0;
          write_buf[0] = '\0';
          state = STATE_MAIL_CONV;
          drawConversation();
          beep(100);
        }
        while (digitalRead(BTN_OK) == LOW) delay(10);
      }
    }
  } else {
    okWasDown = false;
  }
}

void loop() {
  handleStateButtons();
  checkLongPressSend();
  checkSendButton();
  autoFetchInbox();
  delay(30);
}

// --- Tự động kiểm tra tin nhắn mới mỗi 5 giây ---
unsigned long lastInboxCheck = 0;
const unsigned long INBOX_INTERVAL = 5000;
void autoFetchInbox() {
  if (millis() - lastInboxCheck > INBOX_INTERVAL) {
    lastInboxCheck = millis();
    // Chỉ kiểm tra khi đã kết nối WiFi và đang ở các trạng thái liên quan đến mail
    if (WiFi.status() == WL_CONNECTED) {
      // Clear previous received messages
      for (int i = 0; i < mail_count; i++) {
        Conversation &c = convs[i];
        int j = 0;
        for (int k = 0; k < c.size; k++) {
          if (c.msgs[k].startsWith("You: ")) {
            c.msgs[j++] = c.msgs[k];
          }
        }
        c.size = j;
      }
      // Lấy tin nhắn mới cho thiết bị hiện tại
      fetchInboxFromServer(my_device_id);
      // Redraw if necessary
      if (state == STATE_MAIL_LIST) {
        drawMailList();
      } else if (state == STATE_MAIL_CONV) {
        drawConversation();
      }
    }
  }
}