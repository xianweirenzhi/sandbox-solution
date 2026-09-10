#include <Arduino.h>

#include "soil_sensor.h"

// ===================== 参数（改引脚/周期只动这里）=====================
#define SOIL_ADC_PIN  A0       // AO（经分压）接唯一 ADC 引脚（MQ-2 已移除，A0 空出）
#define SOIL_POLL_MS  1000UL   // 采集周期（毫秒）
#define SOIL_SAMPLES  5        // 单周期采样次数（奇数，取中值滤除 WiFi 干扰抖动）
// 超量程提示：长期贴满量程大概率是 AO 直连未分压（3.3V 超 ADC 量程 0~1V），
// 连续 N 个周期贴顶仅串口提醒（接线排障用；湿/干判定已上移主机，此处无阈值逻辑）。
#define SOIL_PEGGED_RAW   1015   // “贴顶”判定下限
#define SOIL_PEGGED_STREAK  10   // 连续贴顶 N 个周期（默认 10s）打印提醒

// ===================== 实现体 =====================
struct SoilSensor::Impl {
  int      raw = 0;            // 最近一次滤波后原始值（0~1023）
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

int SoilSensor::getRaw() const {
  return _p->raw;
}

bool SoilSensor::begin() {
  _p->raw = readMedian();       // 首采即有值（上报/上屏立即可用）
  _p->peggedStreak = 0;
  _p->lastPollMs = millis() - SOIL_POLL_MS;

  Serial.print(F("[soil] 土壤湿度模块就绪（AO→A0，原始值 raw="));
  Serial.print(_p->raw);
  Serial.println(F("；湿/干判定与阈值在主机侧）"));
  return true;  // 模拟器件无可探测的“在线”概念，恒返回 true
}

void SoilSensor::handle() {
  uint32_t now = millis();
  if (now - _p->lastPollMs < SOIL_POLL_MS) return;
  _p->lastPollMs = now;

  _p->raw = readMedian();       // 仅更新滤波原始值，无判定逻辑

  // 贴顶提醒：疑似 AO 直连未分压（读数超出 ADC 量程被削顶）
  if (_p->raw >= SOIL_PEGGED_RAW) {
    if (++_p->peggedStreak == SOIL_PEGGED_STREAK) {
      Serial.println(F("[soil] 长期贴满量程：请确认 AO 已分压（现场为 4:1）且接线正常"));
    }
  } else {
    _p->peggedStreak = 0;
  }
}
