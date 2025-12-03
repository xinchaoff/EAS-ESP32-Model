
/*
  EAS_mini_rocker_valve_demo.ino
  Demo cực nhỏ cho mô hình EAS:
  - Đọc 1 rocker switch (giả sử bật là nối GND, tắt là hở)
  - Điều khiển 1 output (LED/relay/ULN → van)
  - In trạng thái ra Serial để dễ debug

  Mục tiêu của file này:
  - Làm "mẫu" đơn giản để bạn hiểu từng lệnh cơ bản
  - Sau này có thể copy cấu trúc này và mở rộng cho nhiều rocker, nhiều van
*/

// ====== KHAI BÁO CHÂN PHẦN CỨNG (bạn có thể đổi cho đúng mạch thật) ======

// Chân đọc rocker (nút điều khiển) - nối với GPIO qua mạch kéo lên/kéo xuống
const int PIN_ROCKER = 4;      // ví dụ dùng GPIO4 để đọc rocker

// Chân điều khiển van/LED qua ULN/relay
const int PIN_VALVE  = 16;     // ví dụ dùng GPIO16 để điều khiển van/LED

// ====== CÁC BIẾN TOÀN CỤC ĐỂ LƯU TRẠNG THÁI ======

// Biến lưu trạng thái hiện tại của rocker (đã xử lý logic)
bool rockerOn = false;

// Biến lưu trạng thái cũ để tránh in Serial liên tục
bool lastRockerOn = false;


// ====== HÀM setup(): chạy 1 lần khi ESP32 khởi động ======
void setup() {
  // Mở cổng Serial để debug
  // 115200 là tốc độ baud khá phổ biến cho ESP32
  Serial.begin(115200);

  // Thiết lập mode cho các chân
  // Rocker dùng INPUT_PULLUP: bình thường là HIGH, khi bật sẽ kéo xuống LOW (GND)
  pinMode(PIN_ROCKER, INPUT_PULLUP);

  // Chân điều khiển van là OUTPUT: xuất HIGH/LOW để điều khiển ULN/relay/LED
  pinMode(PIN_VALVE, OUTPUT);

  // Tắt van/LED lúc khởi động cho an toàn
  digitalWrite(PIN_VALVE, LOW);

  Serial.println("=== EAS mini demo: rocker -> valve ===");
}


// ====== HÀM loop(): lặp vô hạn, chạy đi chạy lại ======
void loop() {
  // 1. Cập nhật trạng thái rocker
  updateRockerState();

  // 2. Điều khiển van/LED dựa trên trạng thái rocker
  applyValveControl();

  // 3. In ra Serial khi trạng thái thay đổi (để bạn dễ quan sát)
  debugIfStateChanged();

  // Delay nhẹ cho dễ đọc log (có thể bỏ hoặc giảm sau này)
  delay(20);
}


// ====== HÀM: Đọc trạng thái rocker và gán vào biến rockerOn ======
void updateRockerState() {
  // Đọc giá trị thô từ chân PIN_ROCKER: sẽ là HIGH hoặc LOW
  int raw = digitalRead(PIN_ROCKER);

  // Vì ta dùng INPUT_PULLUP:
  // - Khi rocker "không bật": chân ở HIGH (do điện trở kéo lên bên trong ESP32)
  // - Khi rocker "bật": rocker nối chân này xuống GND → đọc được LOW
  //
  // Để dễ hiểu, ta sẽ quy ước:
  //   rockerOn = true  khi raw == LOW (người dùng đang bật/nhấn)
  //   rockerOn = false khi raw == HIGH
  if (raw == LOW) {
    rockerOn = true;
  } else {
    rockerOn = false;
  }
}


// ====== HÀM: Điều khiển van/LED dựa trên biến rockerOn ======
void applyValveControl() {
  if (rockerOn) {
    // Nếu rocker đang bật → bật van/LED
    digitalWrite(PIN_VALVE, HIGH);
  } else {
    // Nếu rocker tắt → tắt van/LED
    digitalWrite(PIN_VALVE, LOW);
  }
}


// ====== HÀM: In trạng thái khi có thay đổi để dễ theo dõi trên Serial ======
void debugIfStateChanged() {
  if (rockerOn != lastRockerOn) {
    // Trạng thái vừa thay đổi so với lần trước
    Serial.print("Rocker: ");
    Serial.print(rockerOn ? "ON  " : "OFF ");
    Serial.print(" | Valve: ");
    Serial.println(rockerOn ? "ON" : "OFF");

    // Cập nhật trạng thái cũ
    lastRockerOn = rockerOn;
  }
}
