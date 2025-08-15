#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "LUMI_VIETNAM";
const char* password = "lumivn274";
const char* serverRegister = "http://10.10.50.204:5000/devices/register";
const char* serverSend = "http://10.10.50.204:5000/messages/send";
const char* serverInbox = "http://10.10.50.204:5000/messages/inbox?device_id=esp_test_01";

String deviceId = "esp_test_02";
String receiverId = "esp_test_01";
String lastMessage = "";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  registerDevice();
  Serial.println("Nhap tin nhan de gui cho esp_test_01:");
}

void loop() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      sendMessage(msg);
    }
  }
  checkNewMessage();
  delay(3000);
}

void registerDevice() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverRegister);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"device_id\":\"" + deviceId + "\",\"name\":\"ESP Test 02\"}";
    int httpResponseCode = http.POST(jsonData);
    Serial.print("Dang ky thiet bi: ");
    Serial.println(httpResponseCode);
    http.end();
  }
}

void sendMessage(String content) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverSend);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"sender_id\":\"" + deviceId + "\",\"receiver_id\":\"" + receiverId + "\",\"content\":\"" + content + "\"}";
    int httpResponseCode = http.POST(jsonData);
    Serial.print("Da gui: ");
    Serial.println(content);
    http.end();
  }
}

void checkNewMessage() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverInbox);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      if (payload != lastMessage && payload.length() > 2) {
        lastMessage = payload;
        Serial.println("Tin nhan moi:");
        Serial.println(payload);
      }
    }
    http.end();
  }
}
