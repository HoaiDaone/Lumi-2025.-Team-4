#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS     11
#define TFT_DC     8
#define TFT_RST    18
#define TFT_MOSI   9
#define TFT_SCLK   10
#define TFT_LED    17
#define BUZZER_PIN 35

#define BTN_BACK    16
#define BTN_NEXT    15
#define BTN_OK      7
#define BTN_CANCEL  6
#define BTN_SEND    0

// WiFi info
const char* ssid = "hht";
const char* password = "25032004";

// IP ESP32-S3 bên kia
const char* target_host = "http://10.80.19.41/msg";  // chỉnh lại cho phù hợp

SPIClass hspi(HSPI);
Adafruit_ST7735 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

char char_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789";
int char_index = 0;
char msg[51] = "";
int msg_len = 0;

unsigned long lastDebounce = 0;

// Hiển thị nội dung tin nhắn nhập
void displayInputScreen() {
  tft.fillScreen(ST77XX_WHITE);
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.println("NHAP TIN:");

  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.setTextColor(ST77XX_MAGENTA);
  tft.println(msg);

  tft.setCursor(10, 60);
  tft.setTextColor(ST77XX_RED);
  tft.print("Ky tu: ");
  tft.println(char_set[char_index]);
}

// Xử lý khi nhận HTTP POST từ ESP khác
void handleMsg() {
  if (server.hasArg("plain")) {
    String content = server.arg("plain");

    // Hiển thị tin nhắn nhận được
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);
    tft.println("NHAN:");

    tft.setCursor(10, 40);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.println(content);

    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  }

  server.send(200, "text/plain", "OK");
}

// Gửi tin nhắn qua HTTP POST
void sendMessage(const char* message) {
  HTTPClient http;
  http.begin(target_host);
  http.addHeader("Content-Type", "text/plain");
  int httpResponseCode = http.POST(message);
  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(TFT_LED, OUTPUT); digitalWrite(TFT_LED, HIGH);

  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);
  pinMode(BTN_SEND, INPUT_PULLUP);

  hspi.begin(TFT_SCLK, -1, TFT_MOSI);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(10, 30);
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(2);
  tft.println("Dang ket noi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }

  tft.println("\nDa ket noi!");
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  server.on("/msg", HTTP_POST, handleMsg);
  server.begin();

  delay(3000);
  displayInputScreen();
}

void loop() {
  server.handleClient();

  if (millis() - lastDebounce > 200) {
    if (!digitalRead(BTN_NEXT)) {
      char_index = (char_index + 1) % strlen(char_set);
      displayInputScreen();
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_BACK)) {
      char_index = (char_index - 1 + strlen(char_set)) % strlen(char_set);
      displayInputScreen();
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_OK)) {
      if (msg_len < 50) {
        msg[msg_len++] = char_set[char_index];
        msg[msg_len] = '\0';
        displayInputScreen();
        Serial.println(msg);
      }
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_CANCEL)) {
      if (msg_len > 0) {
        msg[--msg_len] = '\0';
        displayInputScreen();
        Serial.println(msg);
      }
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_SEND)) {
      Serial.print("Gui: "); Serial.println(msg);
      sendMessage(msg);

      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(10, 10);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(2);
      tft.println("DA GUI:");

      tft.setCursor(10, 40);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.println(msg);

      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);

      delay(2000);
      msg[0] = '\0';
      msg_len = 0;
      displayInputScreen();
      lastDebounce = millis();
    }
  }

  delay(10);
}

