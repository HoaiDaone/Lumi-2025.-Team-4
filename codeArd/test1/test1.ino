void setup() {
  Serial.begin(115200); // Khởi tạo Serial với baudrate 115200
  while (!Serial) {     // Chờ kết nối Serial (chỉ cần thiết với một số board)
    delay(10);
  }
  Serial.println("ESP32 san sang nhan tin nhan...");
  Serial.println("Hay gui tin nhan bat ky qua Serial Monitor!");
}

void loop() {
  if (Serial.available() > 0) { // Kiểm tra có dữ liệu nhận được
    String receivedMessage = Serial.readStringUntil('\n'); // Đọc đến khi gặp ký tự xuống dòng
    receivedMessage.trim(); // Loại bỏ ký tự trắng thừa
    
    Serial.print("Da nhan: "); // In ra màn hình Serial Monitor
    Serial.println(receivedMessage);
    
    Serial.print("Do dai tin nhan: "); // In độ dài tin nhắn
    Serial.println(receivedMessage.length());
    
    Serial.print("Ky tu dau tien: "); // In ký tự đầu tiên (để debug)
    if (receivedMessage.length() > 0) {
      Serial.println(receivedMessage.charAt(0));
    } else {
      Serial.println("(rong)");
    }
  }
  delay(10); // Tránh treo hệ thống
}