// ROCKER_ADC_DEBUG - ESP32-ACTUATOR
// ----------------------------------------------------
// Mục tiêu: đọc raw ADC của 4 rocker (REAR/FRONT/ALL/MODE)
// để xem mỗi vị trí UP / CENTER / DOWN cho ra giá trị bao nhiêu.
// KHÔNG điều khiển ULN, chỉ in ra Serial.
//
// Cách dùng:
// 1. Nạp code cho ESP-ACTUATOR.
// 2. Mở Serial Monitor 115200.
// 3. Lần lượt xoay từng rocker qua 3 nấc, mỗi nấc giữ 1–2 giây.
// 4. Ghi lại dãy số ADC tương ứng (khoảng giá trị) cho từng nấc.
//
// Sau khi có số thực tế, ta sẽ chỉnh lại ngưỡng trong Step 1B.

#include <Arduino.h>

const int PIN_ROCKER_REAR  = 25;
const int PIN_ROCKER_FRONT = 26;
const int PIN_ROCKER_ALL   = 27;
const int PIN_ROCKER_MODE  = 14;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("===== ROCKER ADC DEBUG (ESP32-ACTUATOR) ====="));
  Serial.println(F("Doc ADC 4 rocker: REAR(25), FRONT(26), ALL(27), MODE(14)."));
  Serial.println(F("Hay lan luot de moi rocker o cac vi tri DOWN / CENTER / UP va quan sat gia tri."));
  Serial.println();

  pinMode(PIN_ROCKER_REAR,  INPUT_PULLUP);
  pinMode(PIN_ROCKER_FRONT, INPUT_PULLUP);
  pinMode(PIN_ROCKER_ALL,   INPUT_PULLUP);
  pinMode(PIN_ROCKER_MODE,  INPUT_PULLUP);
}

void loop() {
  int rear  = analogRead(PIN_ROCKER_REAR);
  int front = analogRead(PIN_ROCKER_FRONT);
  int all   = analogRead(PIN_ROCKER_ALL);
  int mode  = analogRead(PIN_ROCKER_MODE);

  Serial.print(F("REAR="));
  Serial.print(rear);
  Serial.print(F(" | FRONT="));
  Serial.print(front);
  Serial.print(F(" | ALL="));
  Serial.print(all);
  Serial.print(F(" | MODE="));
  Serial.println(mode);

  delay(200);
}
