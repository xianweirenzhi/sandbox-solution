#include <Arduino.h>

#include "mq2_sensor.h"

// ===================== 采集参数（改阈值/周期只动这里）=====================
// 原始值刻度：ESP8266 ADC 为 10 位（0~1023 对应 A0 0~1.0V）；经 5×1k/1k 分压
// （5:1），5V 满量程 → 0.83V → 约 850 counts。清洁空气预热后的基线需现场标定
// （6kΩ 负载会拉低 AO 输出，基线通常明显低于 100；报警阈值按实测基线上浮设定）。
#define MQ2_READ_MS       2000UL    // 采集周期（毫秒）
#define MQ2_SAMPLES       5         // 单周期采样次数（奇数，取中值滤除 WiFi 干扰抖动）
#define MQ2_PREHEAT_MS    180000UL  // 预热时长（毫秒，3 分钟；新传感器首次需老化 24~48h）
#define MQ2_ALARM_RAW     300       // 报警阈值（滤波后原始值超过即报 GAS!）
#define MQ2_EDGE_RAW      5         // “贴边”判定下限（<此值视为贴 0）
#define MQ2_EDGE_STREAK   30        // 连续贴边 N 个周期（默认 1 分钟）判读数异常

// ===================== 实现体 =====================
struct Mq2Sensor::Impl {
  int      raw = 0;             // 最近一次滤波后原始值（0~1023）
  bool     preheat = true;      // 是否预热中
  uint32_t startMs = 0;         // begin 时刻（预热计时基准）
  uint32_t lastReadMs = 0;      // 上次采集时刻（限频用）
  uint16_t edgeStreak = 0;      // 连续贴边计数（贴 0 或贴满量程）
  bool     sane = true;         // 读数合理性（长期贴边 → false，恢复即 true）
};

// ===================== 内部小工具 =====================

// 单周期采样：连续采 MQ2_SAMPLES 次取中值（ESP8266 ADC 受 WiFi 射频干扰抖动明显）
static int readMedian(uint8_t n) {
  int s[MQ2_SAMPLES];
  uint8_t cnt = n < MQ2_SAMPLES ? n : MQ2_SAMPLES;
  for (uint8_t i = 0; i < cnt; i++) {
    s[i] = analogRead(A0);
    delayMicroseconds(200);     // 采样间稍作间隔，避免连读相关
  }
  // 插入排序取中值（样本少，直接插入足够）
  for (uint8_t i = 1; i < cnt; i++) {
    int v = s[i];
    int8_t j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  return s[cnt / 2];
}

// ===================== 公开接口 =====================

Mq2Sensor::Mq2Sensor() : _p(new Impl) {}

Mq2Sensor::~Mq2Sensor() {
  delete _p;
  _p = nullptr;
}

bool Mq2Sensor::isPreheat() const {
  return _p->preheat;
}

bool Mq2Sensor::isOk() const {
  return _p->sane;
}

bool Mq2Sensor::isAlarm() const {
  // 预热期读数无意义，不报警；合理性异常时读数不可信，也不报警
  return !_p->preheat && _p->sane && _p->raw > MQ2_ALARM_RAW;
}

int Mq2Sensor::getRaw() const {
  return _p->raw;
}

bool Mq2Sensor::begin() {
  _p->startMs = millis();
  _p->preheat = true;
  _p->edgeStreak = 0;
  _p->sane = true;
  _p->raw = 0;
  _p->lastReadMs = millis() - MQ2_READ_MS;

  Serial.println(F("[mq2] MQ-2 上电预热中（3 分钟），此期间读数无意义"));
  Serial.println(F("[mq2] 接线确认：独立 5V 供电须与开发板共地；AO 经 5×1k/1k 分压接 ADC"));
  return true;  // 模拟器件无可探测的“在线”概念，恒返回 true
}

void Mq2Sensor::handle() {
  // 1) 预热计时
  if (_p->preheat && millis() - _p->startMs >= MQ2_PREHEAT_MS) {
    _p->preheat = false;
    Serial.println(F("[mq2] 预热完成，开始正常判定"));
  }

  // 2) 按周期采集
  uint32_t now = millis();
  if (now - _p->lastReadMs < MQ2_READ_MS) return;
  _p->lastReadMs = now;

  int v = readMedian(MQ2_SAMPLES);
  _p->raw = v;

  // 3) 读数合理性监测：连续多个周期贴 0（疑似 AO 未接/断线）或贴满量程
  //    （疑似分压失效直连 5V）判异常，恢复即自动转好。预热期不参与判定。
  if (_p->preheat) {
    _p->edgeStreak = 0;
  } else if (v < MQ2_EDGE_RAW || v > 1018) {
    if (++_p->edgeStreak >= MQ2_EDGE_STREAK) _p->sane = false;
  } else {
    _p->edgeStreak = 0;
    _p->sane = true;
  }

  Serial.print(F("[mq2] raw="));
  Serial.print(v);
  if (_p->preheat) Serial.println(F(" (HEAT)"));
  else if (isAlarm()) Serial.println(F("  <<< GAS ALARM"));
  else Serial.println();
}
