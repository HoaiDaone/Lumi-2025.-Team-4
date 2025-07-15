// Copy sang Arduino hoặc cài Platform để chạy trên VScode cũng đc

#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#define BUZZER_PIN  13

const char* ssid = "Truong Son 2.4G";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverList = "http://192.168.1.7:5000/devices/list"; 
const char* serverInbox = "http://192.168.1.7:5000/messages/inbox?device_id=";
const char* serverSend = "http://192.168.1.7:5000/messages/send";

TFT_eSPI tft = TFT_eSPI();
String deviceId = "esp32_01";
String selectedDevice = "";
String lastMessage = "";
int frame = 1; 

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  WiFi.begin(ssid, password);
  tft.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  tft.println("OK");

  showFrame1();
}

void loop() {
  if (frame == 1) {
    // Nhập ID thiết bị muốn kết nối qua Serial
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.length() > 0) {
        selectedDevice = input;
        frame = 2;
        showFrame2();
      }
    }
    // TODO: Có thể thêm nút nhấn để chọn hội thoại từ danh sách
  } else if (frame == 2) {
    // Nhập tin nhắn mới qua Serial
    if (Serial.available()) {
      String msg = Serial.readStringUntil('\n');
      msg.trim();
      if (msg.length() > 0 && msg.length() <= 50) {
        sendMessage(msg);
        showFrame2(); // refresh lại frame chat
      }
    }
    // TODO: Thêm nút nhấn để quay lại frame 1
  }
}

void showFrame1() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.println("Nhap ID thiet bi ket noi:");
  tft.println("VD: esp32_02");
  tft.println("Danh sach hoi thoai:");
  // Gọi API lấy danh sách thiết bị (hội thoại)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverList);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      // Giả sử API trả về chuỗi JSON dạng ["esp32_02","esp32_03",...]
      payload.replace('[', ' ');
      payload.replace(']', ' ');
      payload.replace('"', ' ');
      payload.trim();
      int y = tft.getCursorY();
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.println(payload);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      tft.println("Khong lay duoc danh sach!");
    }
    http.end();
  } else {
    tft.println("WiFi loi!");
  }
}

void showFrame2() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.print("Chat voi: ");
  tft.println(selectedDevice);
  tft.println("Lich su tin nhan:");
  // Gọi API lấy lịch sử tin nhắn với selectedDevice
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverInbox) + deviceId + "&with=" + selectedDevice;
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      // Giả sử API trả về chuỗi JSON dạng ["msg1","msg2",...]
      payload.replace('[', ' ');
      payload.replace(']', ' ');
      payload.replace('"', ' ');
      payload.trim();
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.println(payload);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      tft.println("Khong lay duoc tin nhan!");
    }
    http.end();
  } else {
    tft.println("WiFi loi!");
  }
}

void sendMessage(String content) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverSend);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"sender_id\":\"" + deviceId + "\",\"receiver_id\":\"" + selectedDevice + "\",\"content\":\"" + content + "\"}";
    int httpResponseCode = http.POST(jsonData);
    tft.println("Da gui: " + content);
    http.end();
  } else {
    tft.println("WiFi loi!");
  }
}