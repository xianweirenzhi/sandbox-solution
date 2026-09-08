#include <Arduino.h>

#include "soil_sensor.h"

// ===================== 参数（改引脚/阈值/周期只动这里）=====================
#define SOIL_ADC_PIN  A0       // AO（经分压）接唯一 ADC 引脚（MQ-2 已移除，A0 空出）
#define SOIL_POLL_MS  1000UL   // 采集周期（毫秒）
#define SOIL_SAMPLES  5        // 单周期采样次数（奇数，取中值滤除 WiFi 干扰抖动）
// 判定阈值（滤波后原始值 0~1023，对应 A0 0~1.0V；★须现场标定后按需调整）：
// 语义：raw 低 = 湿润适宜，raw 高 = 过干；两阈值之间维持原判定（滞回防抖）。
// 默认值按 5×1kΩ+1kΩ（5:1）分压 + 3V3 供电估算：湿润 ~1.5V→0.25V≈256，
// 过干 ~2.8V→0.47V≈478，取整留出滞回带。
#define SOIL_MOIST_BELOW 300   // 滤波值低于此值 → 判湿润适宜
#define SOIL_DRY_ABOVE   420   // 滤波值高于此值 → 判过干
// 超量程提示：长期贴满量程大概率是 AO 直连未分压（3.3V 超 ADC 量程 0~1V），
// 连续 N 个周期贴顶仅串口提醒（不改变判定，功能上等价于“极干”）。
#define SOIL_PEGGED_RAW   1015   // “贴顶”判定下限
#define SOIL_PEGGED_STREAK  10   // 连续贴顶 N 个周期（默认 10s）打印提醒

// ===================== 实现体 =====================
struct SoilSensor::Impl {
  int      raw = 0;            // 最近一次滤波后原始值（0~1023）
  bool     moist = true;       // 生效判定（begin 时按首采初始化）
  uint32_t lastPollMs = 0;     // 上次采集时刻（限频用）
  uint16_t peggedStreak = 0;   // 连续贴顶计数
};

// ===================== 内部小工具 =====================

// 单周期采样：连续采 SOIL_SAMPLES 次取中值（ESP8266 ADC 受 WiFi 射频干扰抖动明显）
static int readMedian() {
  int s[SOIL_SAMPLES];
  for (uint8_t i = 0; i < SOIL_SAMPLES; i++) {
    s[i] = analogRead(SOIL_ADC_PIN);
    delayMicroseconds(200);    // 采样间稍作间隔，避免连读相关
  }
  // 插入排序取中值（样本少，直接插入足够）
  for (uint8_t i = 1; i < SOIL_SAMPLES; i++) {
    int v = s[i];
    int8_t j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  return s[SOIL_SAMPLES / 2];
}

// ===================== 公开接口 =====================

SoilSensor::SoilSensor() : _p(new Impl) {}

SoilSensor::~SoilSensor() {
  delete _p;
  _p = nullptr;
}

bool SoilSensor::isMoist() const {
  return _p->moist;
}

bool SoilSensor::isDry() const {
  return !isMoist();
}

int SoilSensor::getRaw() const {
  return _p->raw;
}

bool SoilSensor::begin() {
  _p->raw = readMedian();
  _p->moist = _p->raw < SOIL_MOIST_BELOW;  // 开机按首采给初始判定，不误报
  _p->peggedStreak = 0;
  _p->lastPollMs = millis() - SOIL_POLL_MS;

  Serial.print(F("[soil] 土壤湿度模块就绪（AO→A0，初始 raw="));
  Serial.print(_p->raw);
  Serial.print(F("，初始判定: "));
  Serial.print(isMoist() ? F("湿润适宜") : F("湿度过低"));
  Serial.println(F("）"));
  return true;  // 模拟器件无可探测的“在线”概念，恒返回 true
}

void SoilSensor::handle() {
  uint32_t now = millis();
  if (now - _p->lastPollMs < SOIL_POLL_MS) return;
  _p->lastPollMs = now;

  int v = readMedian();
  _p->raw = v;

  // 滞回双阈值判定：低于 MOIST_BELOW 判湿，高于 DRY_ABOVE 判干，
  // 之间维持原判定——临界湿度附近的缓慢漂移不会引起状态翻转。
  if (v < SOIL_MOIST_BELOW && !_p->moist) {
    _p->moist = true;
    Serial.print(F("[soil] 判定切换: 湿润适宜（raw="));
    Serial.print(v);
    Serial.println(F("）"));
  } else if (v > SOIL_DRY_ABOVE && _p->moist) {
    _p->moist = false;
    Serial.print(F("[soil] 判定切换: 湿度过低（raw="));
    Serial.print(v);
    Serial.println(F("）"));
  }

  // 贴顶提醒：疑似 AO 直连未分压（读数超出 ADC 量程被削顶）
  if (v >= SOIL_PEGGED_RAW) {
    if (++_p->peggedStreak == SOIL_PEGGED_STREAK) {
      Serial.println(F("[soil] 长期贴满量程：请确认 AO 已分压（推荐 5:1）且接线正常"));
    }
  } else {
    _p->peggedStreak = 0;
  }
}
