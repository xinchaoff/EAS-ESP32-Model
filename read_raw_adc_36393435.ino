// EAS_4HeightSensors_ADC_Read.ino
// Đọc raw ADC từ 4 cảm biến chiều cao nối vào GPIO36, 39, 34, 35 trên ESP32
// In giá trị ra Serial để debug.
//
// Kết nối (gợi ý):
// - Sensor 1 OUT -> GPIO36 (VP)
// - Sensor 2 OUT -> GPIO39 (VN)
// - Sensor 3 OUT -> GPIO34
// - Sensor 4 OUT -> GPIO35
//
// Lưu ý: tất cả cảm biến và ESP32 phải chung GND.

const int SENSOR1_PIN = 36;  // ADC1_CH0
const int SENSOR2_PIN = 39;  // ADC1_CH3
const int SENSOR3_PIN = 34;  // ADC1_CH6
const int SENSOR4_PIN = 35;  // ADC1_CH7

// Thời gian giữa mỗi lần đọc (ms)
const unsigned long READ_INTERVAL = 200; // 5 lần/giây

unsigned long lastReadTime = 0;

void setup() {
  // Khởi động Serial để xem giá trị ADC trên màn hình Serial Monitor
  Serial.begin(115200);
  delay(500);

  // Cấu hình các chân cảm biến là input (thực ra với analogRead ESP32 không bắt buộc, nhưng ghi cho rõ nghĩa)
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  pinMode(SENSOR3_PIN, INPUT);
  pinMode(SENSOR4_PIN, INPUT);

  // Tuỳ chọn: đặt độ phân giải ADC (ESP32 mặc định 12-bit: 0–4095)
  // analogReadResolution(12);

  Serial.println();
  Serial.println(F("=== EAS 4 Height Sensors - RAW ADC Reader ==="));
  Serial.println(F("Pins: 36, 39, 34, 35 (ADC1)"));
  Serial.println(F("Baud: 115200"));
  Serial.println(F("============================================="));
}

void loop() {
  unsigned long now = millis();

  // Chỉ đọc khi đủ interval (tránh spam quá nhanh)
  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    // Đọc raw ADC từ 4 chân
    int raw1 = analogRead(SENSOR1_PIN);
    int raw2 = analogRead(SENSOR2_PIN);
    int raw3 = analogRead(SENSOR3_PIN);
    int raw4 = analogRead(SENSOR4_PIN);

    // In ra Serial theo 1 dòng cho dễ nhìn / dễ log
    // Định dạng: t(ms);S1;S2;S3;S4
    Serial.print(now);
    Serial.print(F(" ms | ADC36="));
    Serial.print(raw1);
    Serial.print(F("  ADC39="));
    Serial.print(raw2);
    Serial.print(F("  ADC34="));
    Serial.print(raw3);
    Serial.print(F("  ADC35="));
    Serial.println(raw4);

    // Nếu muốn hiển thị theo kiểu CSV để copy qua Excel thì có thể in như này:
    /*
    Serial.print(now); Serial.print(";");
    Serial.print(raw1); Serial.print(";");
    Serial.print(raw2); Serial.print(";");
    Serial.print(raw3); Serial.print(";");
    Serial.println(raw4);
    */
  }

  // Không làm gì khác trong loop, chỉ đọc & in giá trị ADC
}
