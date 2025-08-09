#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Định nghĩa chân kết nối LCD
#define TFT_CS     11
#define TFT_DC     8
#define TFT_RST    18
#define TFT_MOSI   9
#define TFT_SCLK   10
#define TFT_LED    17

// Dùng SPI bus 1 (trên ESP32-S3 là cách an toàn)
SPIClass mySPI(1);

// Khởi tạo LCD ST7735 với SPI tùy chỉnh
Adafruit_ST7735 tft = Adafruit_ST7735(&mySPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);  // Bật đèn nền

  Serial.begin(115200);
  mySPI.begin(TFT_SCLK, -1, TFT_MOSI);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // Xoay ngang

  tft.fillScreen(ST77XX_YELLOW);  // Nền vàng

  tft.setCursor(10, 30);
  tft.setTextColor(ST77XX_BLUE);  // Chữ màu xanh
  tft.setTextSize(2);
  tft.println("Xin chao LinhPhung!");
}


void loop() {
}
