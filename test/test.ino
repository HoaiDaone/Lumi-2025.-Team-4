// ESP32 Messenger UI (4-buttons) + ST7735
// - 4 nút: UP, DOWN, OK, CANCEL
// - Menus: Main -> Mail / WiFi / Help
// - Mail: danh sách mail, xem hội thoại, soạn tin (bằng nút)
// - WiFi: scan, chọn SSID, nhập password (bằng nút)
// - Help: hướng dẫn

#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

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

// Mạng (mặc định)
const char* default_ssid = "P-404B";
const char* default_password = "88888888404B";

// ST7735 (HSPI)
SPIClass hspi(HSPI);
Adafruit_ST7735 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);

// --- Hệ thống trạng thái ---
enum AppState {
  STATE_MAIN_MENU,
  STATE_MAIL_LIST,
  STATE_MAIL_CONV,
  STATE_MAIL_WRITE,
  STATE_WIFI_LIST,
  STATE_WIFI_PASS,
  STATE_HELP
};
AppState state = STATE_MAIN_MENU;

// Các biến điều hướng menu
int main_index = 0;            // 0: Mail, 1: WiFi, 2: Help
int mail_index = 0;
int conv_index = 0;
int wifi_index = 0;
int wifi_pass_pos = 0;         // vị trí con trỏ nhập mật khẩu

// Dữ liệu giả lập mail / conversation
#define MAX_MAILS 6
#define MAX_MESSAGES_PER_1 12
String mail_ids[MAX_MAILS] = {"ESP32_A", "ESP32_B", "ESP32_C", "Lumi", "Device5", "Device6"};
int mail_count = 6;

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

// WiFi scan results (lưu local)
#define MAX_WIFI 12
String wifi_names[MAX_WIFI];
int wifi_count = 0;
String wifi_password_buffer = "";
int wifi_pass_len = 0;
int wifi_input_cursor = 0;
int wifi_char_index = 0; // tương tự char_index nhưng cho WiFi input

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
  if (wifi_char_index >= KB_TOTAL) wifi_char_index = 0;
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

void indexToRowCol(int idx, int &out_row, int &out_col) {
  int acc = 0;
  for (int r = 0; r < KB_ROWS; r++) {
    int len = kb_row_lengths[r];
    if (idx < acc + len) {
      out_row = r;
      out_col = idx - acc;
      return;
    }
    acc += len;
  }
  // fallback
  out_row = 0;
  out_col = 0;
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
  tft.print((main_index == 1) ? "> WiFi Setting" : "  WiFi Setting");

  tft.setCursor(8, 66);
  tft.print((main_index == 2) ? "> Help" : "  Help");
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

// WiFi list screen
void drawWifiList() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("WiFi - Scan");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  if (wifi_count == 0) {
    tft.setCursor(6, 40);
    tft.print("No networks scanned. Press OK to scan.");
  } else {
    int display = min(wifi_count, 5);
    int start = max(0, wifi_index - 2);
    if (start > wifi_count - display) start = max(0, wifi_count - display);
    for (int i = 0; i < display && (start + i) < wifi_count; i++) {
      int idx = start + i;
      tft.setCursor(6, 20 + i*16);
      if (idx == wifi_index) {
        tft.setTextColor(ST77XX_WHITE);
        tft.fillRect(4, 18 + i*16 - 2, 152, 14, ST77XX_BLUE);
        tft.setCursor(6, 20 + i*16);
        tft.print(wifi_names[idx]);
        tft.setTextColor(ST77XX_BLACK);
      } else {
        tft.print(wifi_names[idx]);
      }
    }
  }

  tft.setCursor(6, 110);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("OK: Select  CANCEL: Back");
}

// WiFi password input (uses same keyboard)
void drawWifiPassword() {
  tft.fillScreen(ST77XX_WHITE);
  drawHeader("WiFi - Password");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);

  tft.setCursor(6, 22);
  tft.print("SSID: ");
  tft.print(wifi_names[wifi_index]);

  tft.setCursor(6, 40);
  tft.print("Pass: ");
  tft.print(wifi_password_buffer);

  // Draw keyboard grid (highlight wifi_char_index)
  tft.setTextSize(1);
  int cell_w = 12;
  int cell_h = 12;
  int top_y = 68;
  for (int r = 0; r < KB_ROWS; r++) {
    int row_len = kb_row_lengths[r];
    int row_width = row_len * cell_w;
    int start_x = (160 - row_width) / 2;
    for (int c = 0; c < row_len; c++) {
      int acc = 0;
      for (int rr = 0; rr < r; rr++) acc += kb_row_lengths[rr];
      int idx = acc + c;

      int x = start_x + c * cell_w;
      int y = top_y + r * (cell_h + 2);

      if (idx == wifi_char_index) {
        tft.fillRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLUE);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
        tft.setTextColor(ST77XX_BLACK);
      } else {
        tft.drawRect(x-1, y-1, cell_w+2, cell_h+2, ST77XX_BLACK);
        tft.setCursor(x+2, y+1);
        tft.print(getKeyAtIndex(idx));
      }
    }
  }

  tft.setCursor(6, 118);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("UP/DOWN: Prev/Next  OK: Add  CANCEL: Del/Back");
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

// WiFi scan (synchronous) - fills wifi_names[]
void doWifiScan() {
  wifi_names[0] = "";
  wifi_count = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    wifi_count = 0;
  } else {
    wifi_count = min(n, MAX_WIFI);
    for (int i = 0; i < wifi_count; i++) {
      wifi_names[i] = WiFi.SSID(i);
    }
  }
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

// Send message stub (you can replace with HTTP POST to target)
void sendMessageNetwork(const char *payload, const String &toId) {
  String s = String("You: ") + String(payload);
  pushMessageToConv(mail_index, s);
  // Emulate reply for demo
  String r = String("Other: OK got '") + String(payload) + String("'");
  pushMessageToConv(mail_index, r);
}

// --- Setup & loop ---
void setupPins() {
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(TFT_LED, OUTPUT); digitalWrite(TFT_LED, HIGH);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);
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

  // demo messages
  pushMessageToConv(0, "Friend: Hello!");
  pushMessageToConv(0, "You: Hi!");

  drawMainMenu();
}

// State machine variables for repeating press handling
void handleStateButtons() {
  if (millis() - lastDebounce < DEBOUNCE_MS) return;

  if (pressedUp()) {
    lastDebounce = millis();
    beep(30);
    switch (state) {
      case STATE_MAIN_MENU:
        main_index = (main_index - 1 + 3) % 3;
        drawMainMenu();
        break;
      case STATE_MAIL_LIST:
        mail_index = max(0, mail_index - 1);
        drawMailList();
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
      case STATE_WIFI_LIST:
        if (wifi_count > 0) {
          wifi_index = max(0, wifi_index - 1);
        }
        drawWifiList();
        break;
      case STATE_WIFI_PASS:
        if (KB_TOTAL > 0) {
          wifi_char_index = (wifi_char_index - 1 + KB_TOTAL) % KB_TOTAL;
        }
        drawWifiPassword();
        break;
      case STATE_HELP:
        break;
    }
  } else if (pressedDown()) {
    lastDebounce = millis();
    beep(30);
    switch (state) {
      case STATE_MAIN_MENU:
        main_index = (main_index + 1) % 3;
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
      case STATE_WIFI_LIST:
        if (wifi_count > 0) wifi_index = min(wifi_count - 1, wifi_index + 1);
        drawWifiList();
        break;
      case STATE_WIFI_PASS:
        if (KB_TOTAL > 0) {
          wifi_char_index = (wifi_char_index + 1) % KB_TOTAL;
        }
        drawWifiPassword();
        break;
      case STATE_HELP:
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
        } else if (main_index == 1) {
          state = STATE_WIFI_LIST;
          drawWifiList();
        } else {
          state = STATE_HELP;
          drawHelp();
        }
        break;
      case STATE_MAIL_LIST:
        state = STATE_MAIL_CONV;
        drawConversation();
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
      case STATE_WIFI_LIST:
        if (wifi_count > 0) {
          state = STATE_WIFI_PASS;
          wifi_password_buffer = "";
          wifi_pass_len = 0;
          wifi_char_index = 0;
          drawWifiPassword();
        } else {
          doWifiScan();
          drawWifiList();
        }
        break;
      case STATE_WIFI_PASS:
        if ((int)wifi_password_buffer.length() < 60 && KB_TOTAL > 0) {
          wifi_password_buffer += getKeyAtIndex(wifi_char_index);
        }
        drawWifiPassword();
        break;
      case STATE_HELP:
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
      case STATE_WIFI_LIST:
        state = STATE_MAIN_MENU;
        drawMainMenu();
        break;
      case STATE_WIFI_PASS:
        if (wifi_password_buffer.length() > 0) {
          wifi_password_buffer.remove(wifi_password_buffer.length() - 1);
          drawWifiPassword();
        } else {
          state = STATE_WIFI_LIST;
          drawWifiList();
        }
        break;
      case STATE_HELP:
        state = STATE_MAIN_MENU;
        drawMainMenu();
        break;
    }
  }
}

// Long-press OK for sending message / connecting WiFi (kept as before)
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
        } else if (state == STATE_WIFI_PASS && wifi_password_buffer.length() > 0) {
          tft.fillScreen(ST77XX_WHITE);
          drawHeader("WiFi - Connecting");
          tft.setCursor(6, 30);
          tft.setTextColor(ST77XX_BLACK);
          tft.print("Connecting to ");
          tft.println(wifi_names[wifi_index]);
          WiFi.begin(wifi_names[wifi_index].c_str(), wifi_password_buffer.c_str());
          unsigned long start = millis();
          while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
            delay(200);
            tft.print(".");
          }
          if (WiFi.status() == WL_CONNECTED) {
            tft.println();
            tft.print("OK IP:");
            tft.println(WiFi.localIP());
          } else {
            tft.println();
            tft.println("Failed");
          }
          delay(900);
          state = STATE_WIFI_LIST;
          drawWifiList();
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
  delay(30);
}
