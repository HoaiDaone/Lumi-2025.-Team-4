#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Định nghĩa chân kết nối theo sơ đồ mạch (GPIO)
#define TFT_CS     11
#define TFT_DC     8
#define TFT_RST    18
#define TFT_MOSI   9
#define TFT_SCLK   10
#define TFT_LED    17
#define BUZZER_PIN 35

// Sử dụng SPI hardware (HSPI)
SPIClass hspi(HSPI);
Adafruit_ST7735 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);

// Biến lưu trữ chuỗi tin nhắn
String receivedString;

void setup() {
  // Khởi tạo Serial
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // Thiết lập buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Bật đèn nền màn hình
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Khởi tạo SPI với các chân SCLK, MISO, MOSI
  hspi.begin(TFT_SCLK, -1, TFT_MOSI); // MISO không sử dụng (-1)

  // Khởi tạo màn hình
  Serial.println("Dang khoi tao man hinh...");
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(1); // Xoay ngang
  tft.fillScreen(ST77XX_BLACK); // Xóa màn hình ban đầu

  // Hiển thị thông báo ban đầu
  tft.setCursor(10, 30);
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(2);
  tft.println("Xin chao!");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("Cho tin nhan...");

  Serial.println("San sang nhan tin nhan...");
}

void loop() {
  while (Serial.available() > 0) {
    char incomingChar = Serial.read();
    
    if (incomingChar == '\n') {
      // Đã nhận đủ chuỗi, tiến hành xử lý
      receivedString.trim();
      
      Serial.print("Nhan duoc: ");
      Serial.println(receivedString);

      // Xóa màn hình và hiển thị tin nhắn mới
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(10, 10);
      tft.setTextColor(ST77XX_BLUE);
      tft.setTextSize(2);
      tft.println("TIN NHAN MOI:");

      tft.setCursor(10, 40);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.setTextWrap(true);
      tft.println(receivedString);

      // Kích hoạt buzzer
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);

      Serial.println("Da hien thi tin nhan len man hinh");
      
      // Xóa chuỗi tạm để chuẩn bị cho tin nhắn tiếp theo
      receivedString = "";
    } else {
      // Nếu không phải ký tự xuống dòng, thêm vào chuỗi
      receivedString += incomingChar;
    }
  }
  delay(10); // Giảm tải cho CPU
}