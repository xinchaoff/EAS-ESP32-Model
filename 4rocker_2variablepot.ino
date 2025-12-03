// EAS_Step2_AutoHeight_V1.ino
// ESP32 ACTUATOR: 4 rocker (MODE/FRONT/REAR/ALL) + 8 ULN + 4 cảm biến chiều cao
// - MANUAL: giữ y như V4_simplified (rocker điều khiển từng bánh).
// - NORMAL: tự cân mức NORMAL_LEVEL (bật UP/DOWN tùy chiều cao).
// - HIGH: tự nâng lên HIGH_LEVEL (chỉ bật UP, không tự DOWN).

#include <Arduino.h>

// ================== CẤU HÌNH CẢM BIẾN CHIỀU CAO ===================
// Mapping 4 cảm biến chiều cao:
//  - POT1 wiper -> GPIO36 (FL) & GPIO39 (FR)
//  - POT2 wiper -> GPIO34 (RL) & GPIO35 (RR)
// Bạn đã xoay cùng chiều kim đồng hồ = ADC tăng từ ~0 -> ~4095
// Giả định: ADC THẤP  = gầm THẤP
//           ADC CAO   = gầm CAO

const int PIN_HS_FL = 36;  // Height Sensor Front Left
const int PIN_HS_FR = 39;  // Height Sensor Front Right
const int PIN_HS_RL = 34;  // Height Sensor Rear Left
const int PIN_HS_RR = 35;  // Height Sensor Rear Right;

// Target & deadband (SẼ CHỈNH SAU KHI ĐO THỰC TẾ)
const int NORMAL_LEVEL = 2050;  // Mức "NORMAL" tạm thời (ADC)
const int HIGH_LEVEL   = 3480;  // Mức "HIGH" tạm thời (ADC)
const int HEIGHT_DEADBAND = 200; // Vùng chết ±ADC quanh target

// Số mẫu để lấy trung bình cho mỗi channel
const int HEIGHT_ADC_SAMPLES = 16;

// ================== MAPPING ULN & ROCKER (GIỮ NHƯ V4_simplified) ==================

// Rocker inputs (3 vị trí, đo ADC thang điện trở)
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

// ---------- NGƯỠNG ADC CHO ROCKER 3 MỨC (Y NHƯ TRƯỚC) ----------
// ADC 0..4095
//  DOWN   : adc < 100
//  UP     : 100 <= adc < 3500
//  CENTER : adc >= 3500

const int ADC_THR_DOWN_UP   = 100;
const int ADC_THR_UP_CENTER = 3500;

// ---------- HEIGHT SENSORS ----------

enum WheelIndex {
  WHEEL_FL = 0,
  WHEEL_FR,
  WHEEL_RL,
  WHEEL_RR,
  WHEEL_COUNT
};

const char *wheelNames[WHEEL_COUNT] = {
  "FL", "FR", "RL", "RR"
};

const uint8_t heightPins[WHEEL_COUNT] = {
  PIN_HS_FL, PIN_HS_FR, PIN_HS_RL, PIN_HS_RR
};

// mapping wheel -> index ULN
const uint8_t wheelUpIndex[WHEEL_COUNT] = {
  0, // FL -> UP_IN1
  1, // FR -> UP_IN2
  2, // RL -> UP_IN3
  3  // RR -> UP_IN4
};
const uint8_t wheelDnIndex[WHEEL_COUNT] = {
  4, // FL -> DN_IN1
  5, // FR -> DN_IN2
  6, // RL -> DN_IN3
  7  // RR -> DN_IN4
};

// lưu raw height ADC
int heightRaw[WHEEL_COUNT];

// ---------- DEBUG ----------

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 2000; // ms

// =============== HÀM TIỆN ÍCH CHUNG =================

int readAdcAveraged(uint8_t pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return (int)(sum / samples);
}

Rocker3Pos readRocker3Pos(uint8_t pin, int &outAdc) {
  int adc = readAdcAveraged(pin, 8); // rocker không cần quá mượt
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

// Đọc toàn bộ 4 cảm biến chiều cao
void readHeightSensors() {
  for (int i = 0; i < WHEEL_COUNT; i++) {
    heightRaw[i] = readAdcAveraged(heightPins[i], HEIGHT_ADC_SAMPLES);
  }
}

void printStatus() {
  Serial.println(F("------ STATUS STEP 2 (AutoHeight_V1) ------"));

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

  Serial.println(F("Height raw (ADC):"));
  for (int i = 0; i < WHEEL_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(wheelNames[i]);
    Serial.print(F(": "));
    Serial.println(heightRaw[i]);
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

// ================== LOGIC CÁC CHẾ ĐỘ ==================

// MANUAL MODE – y như V4_simplified: 3 rocker REAR/FRONT/ALL điều khiển từng bánh
void updateManualMode() {
  allUlnOff();

  if (!allEnable) return;

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

// NORMAL MODE – giữ mức NORMAL_LEVEL với hysteresis ±HEIGHT_DEADBAND
void updateNormalMode() {
  allUlnOff();

  if (!allEnable) return;

  for (int w = 0; w < WHEEL_COUNT; w++) {
    int h = heightRaw[w];

    bool up = false;
    bool dn = false;

    if (h < NORMAL_LEVEL - HEIGHT_DEADBAND) {
      // gầm thấp hơn NORMAL -> nâng lên
      up = true;
    } else if (h > NORMAL_LEVEL + HEIGHT_DEADBAND) {
      // gầm cao hơn NORMAL -> hạ xuống
      dn = true;
    } else {
      // nằm trong vùng NORMAL -> không làm gì
    }

    if (up && !dn) {
      setUln(wheelUpIndex[w], true);
    } else if (dn && !up) {
      setUln(wheelDnIndex[w], true);
    }
  }
}

// HIGH MODE – nâng lên HIGH_LEVEL (chỉ bật UP, không tự DOWN)
void updateHighMode() {
  allUlnOff();

  if (!allEnable) return;

  for (int w = 0; w < WHEEL_COUNT; w++) {
    int h = heightRaw[w];

    bool up = false;

    if (h < HIGH_LEVEL - HEIGHT_DEADBAND) {
      // chưa đủ cao -> tiếp tục UP
      up = true;
    } else {
      // đã gần đến HIGH_LEVEL -> tắt
      up = false;
    }

    if (up) {
      setUln(wheelUpIndex[w], true);
    }
  }
}

// ================== SETUP & LOOP ==================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("===== EAS STEP 2 (AutoHeight_V1) - ACTUATOR + ULN + 4 ROCKER + 4 HEIGHT SENSORS ====="));
  Serial.println(F("Rocker threshold: DOWN < 100, UP 100..3499, CENTER >= 3500."));
  Serial.println(F("Height target (tạm): NORMAL_LEVEL=2000, HIGH_LEVEL=3200, DB=80."));
  Serial.println();

  // Rocker pins: INPUT (đừng dùng PULLUP để khỏi phá thang trở)
  for (int i = 0; i < ROCKER_COUNT; i++) {
    pinMode(rockerPins[i], INPUT);
    int dummy;
    rockerPos[i]     = readRocker3Pos(rockerPins[i], dummy);
    lastRockerPos[i] = rockerPos[i];
  }

  // Height sensor pins
  for (int i = 0; i < WHEEL_COUNT; i++) {
    pinMode(heightPins[i], INPUT);
  }
  readHeightSensors();

  // ULN outputs
  for (int i = 0; i < ULN_COUNT; i++) {
    pinMode(ulnPins[i], OUTPUT);
    digitalWrite(ulnPins[i], LOW);
  }

  currentMode = calcModeFromRocker();
  lastMode    = currentMode;

  printStatus();
}

void loop() {
  // 1) Đọc rocker, log nếu có thay đổi
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

  // 3) Đọc chiều cao
  readHeightSensors();

  // 4) Chạy logic theo mode
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

  // 5) In status định kỳ
  unsigned long now = millis();
  if (now - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = now;
    printStatus();
  }

  delay(10);
}
