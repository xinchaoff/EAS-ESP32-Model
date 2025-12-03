// EAS STEP 1B (V4_simplified) - ACTUATOR + ULN + 4 ROCKER
// Không dùng công tắc ALL_ENABLE, luôn cho phép điều khiển ULN
// Logic hành động dựa trên file logic_hanh_dong:
//  - 4 rocker: MODE / REAR / FRONT / ALL
//  - 8 kênh ULN: UP/DOWN cho FL, FR, RL, RR

#include <Arduino.h>

// ---------- MAPPING PIN TRÊN ESP32-ACTUATOR ----------

// Rocker inputs (3 vị trí, đọc ADC)
const int PIN_ROCKER_REAR  = 25;   // REAR U/D
const int PIN_ROCKER_FRONT = 26;   // FRONT U/D
const int PIN_ROCKER_ALL   = 27;   // ALL U/D
const int PIN_ROCKER_MODE  = 14;   // MODE (HIGH / NORMAL / MANUAL)

// ULN-UP (nâng)
const int PIN_UP_IN1 = 23;  // FL UP
const int PIN_UP_IN2 = 22;  // FR UP
const int PIN_UP_IN3 = 21;  // RL UP
const int PIN_UP_IN4 = 19;  // RR UP

// ULN-DOWN (hạ)
const int PIN_DN_IN1 = 18;  // FL DOWN
const int PIN_DN_IN2 = 5;   // FR DOWN
const int PIN_DN_IN3 = 17;  // RL DOWN
const int PIN_DN_IN4 = 16;  // RR DOWN

// Gom ULN vào mảng (index 0..7)
const uint8_t ulnPins[] = {
  PIN_UP_IN1, // 0 - UP_FL
  PIN_UP_IN2, // 1 - UP_FR
  PIN_UP_IN3, // 2 - UP_RL
  PIN_UP_IN4, // 3 - UP_RR
  PIN_DN_IN1, // 4 - DN_FL
  PIN_DN_IN2, // 5 - DN_FR
  PIN_DN_IN3, // 6 - DN_RL
  PIN_DN_IN4  // 7 - DN_RR
};

const char *ulnNames[] = {
  "UP_FL", "UP_FR", "UP_RL", "UP_RR",
  "DN_FL", "DN_FR", "DN_RL", "DN_RR"
};

const int ULN_COUNT = sizeof(ulnPins) / sizeof(ulnPins[0]);

// ---------- ROCKER & MODE ----------

enum RockerIndex {
  ROCKER_REAR = 0,
  ROCKER_FRONT,
  ROCKER_ALL,
  ROCKER_MODE,
  ROCKER_COUNT
};

const uint8_t rockerPins[ROCKER_COUNT] = {
  PIN_ROCKER_REAR,
  PIN_ROCKER_FRONT,
  PIN_ROCKER_ALL,
  PIN_ROCKER_MODE
};

const char *rockerNames[ROCKER_COUNT] = {
  "REAR",
  "FRONT",
  "ALL",
  "MODE"
};

// 3 trạng thái rocker
enum Rocker3Pos {
  POS_DOWN = -1,
  POS_CENTER = 0,
  POS_UP = 1
};

Rocker3Pos rockerPos[ROCKER_COUNT];
Rocker3Pos lastRockerPos[ROCKER_COUNT];

// Hệ thống mode dựa trên rocker MODE
enum SystemMode {
  MODE_MANUAL = 0,
  MODE_NORMAL,
  MODE_HIGH
};

SystemMode currentMode = MODE_NORMAL;
SystemMode lastMode    = MODE_NORMAL;

// ALL_ENABLE: bản này luôn ON
bool allEnable     = true;
bool lastAllEnable = true;

// ---------- THRESHOLD ADC 3 MỨC (như bản trước) ----------
// ADC 0..4095
//  DOWN   : adc < 100
//  UP     : 100 <= adc < 3500
//  CENTER : adc >= 3500

const int ADC_THR_DOWN_UP   = 100;
const int ADC_THR_UP_CENTER = 3500;

// ---------- DEBUG ----------

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 2000; // ms

// ---------- HÀM TIỆN ÍCH ----------

Rocker3Pos readRocker3Pos(uint8_t pin, int &outAdc) {
  int adc = analogRead(pin);
  outAdc = adc;

  if (adc < ADC_THR_DOWN_UP) {
    return POS_DOWN;
  } else if (adc < ADC_THR_UP_CENTER) {
    return POS_UP;
  } else {
    return POS_CENTER;
  }
}

const char* posToText(Rocker3Pos p) {
  switch (p) {
    case POS_UP: return "UP";
    case POS_DOWN: return "DOWN";
    case POS_CENTER:
    default: return "CENTER";
  }
}

void allUlnOff() {
  for (int i = 0; i < ULN_COUNT; i++) {
    digitalWrite(ulnPins[i], LOW);
  }
}

void setUln(int index, bool on) {
  if (index < 0 || index >= ULN_COUNT) return;
  digitalWrite(ulnPins[index], on ? HIGH : LOW);
}

void printStatus() {
  Serial.println(F("------ STATUS STEP 1B (V4_simplified) ------"));

  Serial.print(F("Mode: "));
  switch (currentMode) {
    case MODE_MANUAL: Serial.println(F("MANUAL")); break;
    case MODE_NORMAL: Serial.println(F("NORMAL")); break;
    case MODE_HIGH:   Serial.println(F("HIGH"));   break;
  }

  Serial.print(F("ALL_ENABLE: "));
  Serial.println(allEnable ? F("ON") : F("OFF"));

  Serial.println(F("Rocker state:"));
  for (int i = 0; i < ROCKER_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(rockerNames[i]);
    Serial.print(F(" -> "));
    Serial.println(posToText(rockerPos[i]));
  }

  Serial.println(F("ULN outputs:"));
  for (int i = 0; i < ULN_COUNT; i++) {
    int v = digitalRead(ulnPins[i]);
    Serial.print(F("  "));
    Serial.print(ulnNames[i]);
    Serial.print(F(" -> "));
    Serial.println(v ? F("ON") : F("OFF"));
  }

  Serial.println(F("---------------------------------------------------"));
}

// Tính mode từ rocker MODE
SystemMode calcModeFromRocker() {
  Rocker3Pos m = rockerPos[ROCKER_MODE];
  // Giữ mapping như trước:
  //   MODE DOWN   -> HIGH
  //   MODE UP     -> MANUAL
  //   MODE CENTER -> NORMAL
  if (m == POS_DOWN) return MODE_HIGH;
  if (m == POS_UP)   return MODE_MANUAL;
  return MODE_NORMAL;
}

// ---------- LOGIC CÁC CHẾ ĐỘ ----------

// MANUAL MODE – 3 rocker REAR/FRONT/ALL điều khiển từng bánh
void updateManualMode() {
  allUlnOff();

  // Cờ lệnh cho từng bánh
  bool upFL = false, upFR = false, upRL = false, upRR = false;
  bool dnFL = false, dnFR = false, dnRL = false, dnRR = false;

  // FRONT rocker
  if (rockerPos[ROCKER_FRONT] == POS_UP) {
    upFL = true;
    upFR = true;
  } else if (rockerPos[ROCKER_FRONT] == POS_DOWN) {
    dnFL = true;
    dnFR = true;
  }

  // REAR rocker
  if (rockerPos[ROCKER_REAR] == POS_UP) {
    upRL = true;
    upRR = true;
  } else if (rockerPos[ROCKER_REAR] == POS_DOWN) {
    dnRL = true;
    dnRR = true;
  }

  // ALL rocker
  if (rockerPos[ROCKER_ALL] == POS_UP) {
    upFL = upFR = upRL = upRR = true;
  } else if (rockerPos[ROCKER_ALL] == POS_DOWN) {
    dnFL = dnFR = dnRL = dnRR = true;
  }

  // Nếu vừa UP vừa DOWN cho 1 bánh -> tắt (tránh xung đột)
  auto applyWheel = [](bool up, bool dn, int upIndex, int dnIndex) {
    if (up && !dn) {
      setUln(upIndex, true);
    } else if (dn && !up) {
      setUln(dnIndex, true);
    }
  };

  // FL: UP_IN1 (0), DN_IN1 (4)
  applyWheel(upFL, dnFL, 0, 4);
  // FR: UP_IN2 (1), DN_IN2 (5)
  applyWheel(upFR, dnFR, 1, 5);
  // RL: UP_IN3 (2), DN_IN3 (6)
  applyWheel(upRL, dnRL, 2, 6);
  // RR: UP_IN4 (3), DN_IN4 (7)
  applyWheel(upRR, dnRR, 3, 7);
}

// NORMAL MODE – STEP 1B: tắt hết ULN (sau này thêm logic cảm biến)
void updateNormalMode() {
  allUlnOff();
}

// HIGH MODE – nâng cả 4 bánh (4 kênh UP bật)
void updateHighMode() {
  allUlnOff();

  // Nâng cả 4 bánh
  setUln(0, true); // FL UP
  setUln(1, true); // FR UP
  setUln(2, true); // RL UP
  setUln(3, true); // RR UP

  // Thực tế: sẽ tắt khi cảm biến báo đủ chiều cao
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("===== EAS STEP 1B (V4_simplified) - ACTUATOR + ULN + 4 ROCKER ====="));
  Serial.println(F("Threshold ADC: DOWN < 100, UP 100..3499, CENTER >= 3500."));
  Serial.println();

  // Rocker pins: dùng INPUT (không kéo PULLUP để khỏi ảnh hưởng thang điện trở)
  for (int i = 0; i < ROCKER_COUNT; i++) {
    pinMode(rockerPins[i], INPUT);
    int dummy;
    rockerPos[i]     = readRocker3Pos(rockerPins[i], dummy);
    lastRockerPos[i] = rockerPos[i];
  }

  // ULN outputs
  for (int i = 0; i < ULN_COUNT; i++) {
    pinMode(ulnPins[i], OUTPUT);
    digitalWrite(ulnPins[i], LOW);
  }

  // Mode ban đầu
  currentMode = calcModeFromRocker();
  lastMode    = currentMode;

  printStatus();
}

// ---------- LOOP ----------

void loop() {
  // 1) Đọc rocker, báo event khi đổi pos
  for (int i = 0; i < ROCKER_COUNT; i++) {
    int adcRaw = 0;
    Rocker3Pos p = readRocker3Pos(rockerPins[i], adcRaw);
    rockerPos[i] = p;

    if (p != lastRockerPos[i]) {
      lastRockerPos[i] = p;
      Serial.print(F("[EVENT] "));
      Serial.print(rockerNames[i]);
      Serial.print(F(" -> "));
      Serial.print(posToText(p));
      Serial.print(F(" (ADC="));
      Serial.print(adcRaw);
      Serial.println(F(")"));
    }
  }

  // 2) Cập nhật mode từ rocker MODE
  currentMode = calcModeFromRocker();
  if (currentMode != lastMode) {
    Serial.print(F("[EVENT] MODE -> "));
    switch (currentMode) {
      case MODE_MANUAL: Serial.println(F("MANUAL")); break;
      case MODE_NORMAL: Serial.println(F("NORMAL")); break;
      case MODE_HIGH:   Serial.println(F("HIGH"));   break;
    }
    lastMode = currentMode;
    allUlnOff();
  }

  // 3) Chạy logic theo mode
  switch (currentMode) {
    case MODE_MANUAL:
      updateManualMode();
      break;
    case MODE_NORMAL:
      updateNormalMode();
      break;
    case MODE_HIGH:
      updateHighMode();
      break;
  }

  // 4) In status định kỳ
  unsigned long now = millis();
  if (now - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = now;
    printStatus();
  }

  delay(10);
}
