#include <Arduino.h>

#include "soil_sensor.h"

// ===================== 参数（改引脚/极性/消抖只动这里）=====================
#define SOIL_DO_PIN        14       // DO 接 GPIO14（避开 boot 约束脚 0/2/15）
// 判定极性：0 = 高电平过干/低电平湿润适宜（现场实测模块如此，2026-09-07 校正）
#define SOIL_HIGH_IS_MOIST 0
#define SOIL_DEBOUNCE_MS   1000UL   // 环境消抖：新电平连续稳定此时长才生效
#define SOIL_POLL_MS       100UL    // 读取周期（毫秒）

// ===================== 实现体 =====================
struct SoilSensor::Impl {
  int      stable = HIGH;        // 已消抖的生效电平（begin 时取当前值）
  int      raw = HIGH;           // 最近一次原始电平
  bool     pending = false;      // 是否存在待生效的偏离电平
  uint32_t pendingMs = 0;        // 偏离电平首次出现时刻（消抖计时基准）
  uint32_t lastPollMs = 0;       // 上次读取时刻（限频用）
};

// ===================== 公开接口 =====================

SoilSensor::SoilSensor() : _p(new Impl) {}

SoilSensor::~SoilSensor() {
  delete _p;
  _p = nullptr;
}

bool SoilSensor::isMoist() const {
#if SOIL_HIGH_IS_MOIST
  return _p->stable == HIGH;
#else
  return _p->stable == LOW;
#endif
}

bool SoilSensor::isDry() const {
  return !isMoist();
}

int SoilSensor::rawLevel() const {
  return _p->raw;
}

bool SoilSensor::begin() {
  pinMode(SOIL_DO_PIN, INPUT);
  _p->raw = digitalRead(SOIL_DO_PIN);
  _p->stable = _p->raw;            // 开机以当前电平为初始稳定态，不误报
  _p->pending = false;
  _p->lastPollMs = millis() - SOIL_POLL_MS;

  Serial.print(F("[soil] 土壤湿度模块就绪（DO=GPIO14，初始判定: "));
  Serial.print(isMoist() ? F("湿润适宜") : F("湿度过低"));
  Serial.println(F("）"));
  return true;  // 数字量两态均合法，无“在线”探测概念
}

void SoilSensor::handle() {
  uint32_t now = millis();
  if (now - _p->lastPollMs < SOIL_POLL_MS) return;
  _p->lastPollMs = now;

  // 读取原始电平并做环境消抖：
  //   原始电平与生效电平一致 → 撤销待生效偏离（跳变被吸收）；
  //   偏离首次出现 → 开始计时；
  //   同一偏离连续保持 ≥ SOIL_DEBOUNCE_MS → 提交为新生效状态。
  //   临界湿度下高频跳变（周期 < 消抖时长）永远凑不满计时期，状态不翻转。
  int raw = digitalRead(SOIL_DO_PIN);
  if (raw != _p->raw) {
    Serial.print(F("[soil] 原始电平跳变: "));
    Serial.println(raw == HIGH ? F("HIGH") : F("LOW"));
  }
  _p->raw = raw;

  if (raw == _p->stable) {
    _p->pending = false;               // 跳回生效值，偏离作废
    return;
  }
  if (!_p->pending) {
    _p->pending = true;                // 偏离首次出现，开始消抖计时
    _p->pendingMs = now;
    return;
  }
  if (now - _p->pendingMs >= SOIL_DEBOUNCE_MS) {
    _p->stable = raw;                  // 持续 1s，提交为新状态
    _p->pending = false;
    Serial.print(F("[soil] 判定切换: "));
    Serial.println(isMoist() ? F("湿润适宜") : F("湿度过低"));
  }
}
