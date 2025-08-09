#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Chân màn hình & buzzer
#define TFT_CS     11
#define TFT_DC     8
#define TFT_RST    18
#define TFT_MOSI   9
#define TFT_SCLK   10
#define TFT_LED    17
#define BUZZER_PIN 35

// Nút bấm
#define BTN_BACK    16
#define BTN_NEXT    15
#define BTN_OK      7
#define BTN_CANCEL  6
#define BTN_SEND    0  // Nút gửi tin nhắn

SPIClass hspi(HSPI);
Adafruit_ST7735 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);

// Nhập liệu
char char_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789";
int char_index = 0;
char msg[51] = "";
int msg_len = 0;

unsigned long lastDebounce = 0;

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

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

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
  tft.println("Xin chao!");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("Nhan phim de nhap...");

  delay(1500);
  displayInputScreen();
}

void loop() {
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
        Serial.print("Da nhap: "); Serial.println(msg);
      }
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_CANCEL)) {
      if (msg_len > 0) {
        msg[--msg_len] = '\0';
        displayInputScreen();
        Serial.print("Da xoa: "); Serial.println(msg);
      }
      lastDebounce = millis();
    }
    if (!digitalRead(BTN_SEND)) {
      Serial.print("Gui tin nhan: ");
      Serial.println(msg);

      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(10, 10);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(2);
      tft.println("DA GUI:");\

      tft.setCursor(10, 40);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.setTextWrap(true);
      tft.println(msg);

      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);

      delay(3000);  // Cho người dùng đọc

      // XÓA tin nhắn sau khi gửi
      msg[0] = '\0';
      msg_len = 0;

      displayInputScreen();
      lastDebounce = millis();
    }
  }

  delay(10);
}
