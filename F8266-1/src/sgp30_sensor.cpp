#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "sgp30_sensor.h"

// ===================== 采集参数（改周期/阈值只动这里）=====================
#define SGP30_I2C_ADDR    0x58     // SGP30 固定 I2C 地址（无地址选择引脚）
#define SGP30_READ_MS     2000UL   // 采集周期（毫秒；datasheet 要求 ≥1s）
#define SGP30_WARMUP_MS   15000UL  // 上电暖机时长（此期间读数不可靠，不判故障）
#define SGP30_FAIL_STREAK 3        // 连续失败 N 次判定传感器故障
#define SGP30_RESCAN_MS   10000UL  // 未检测到传感器时的重扫周期（热插拔自愈）

// ===================== 实现体 =====================
struct Sgp30Sensor::Impl {
  Adafruit_SGP30 sgp = Adafruit_SGP30();
  bool     present = false;    // 总线上是否检测到传感器
  bool     dataValid = false;  // 是否有可信读数
  uint32_t eco2 = 0;           // 最近一次有效 eCO₂（ppm）
  uint32_t tvoc = 0;           // 最近一次有效 TVOC（ppb）
  uint8_t  failStreak = 0;     // 连续读取失败计数（成功清零）
  uint32_t startMs = 0;        // begin 时刻（暖机计时基准）
  uint32_t lastReadMs = 0;     // 上次采集时刻（限频用）
  uint32_t lastScanMs = 0;     // 上次重扫时刻（未在总线时用）
};

// ===================== 公开接口 =====================

Sgp30Sensor::Sgp30Sensor() : _p(new Impl) {}

Sgp30Sensor::~Sgp30Sensor() {
  delete _p;
  _p = nullptr;
}

bool Sgp30Sensor::isOk() const {
  return _p->present && _p->failStreak < SGP30_FAIL_STREAK;
}

bool Sgp30Sensor::isWarmup() const {
  return millis() - _p->startMs < SGP30_WARMUP_MS;
}

bool Sgp30Sensor::hasData() const {
  return _p->dataValid;
}

uint32_t Sgp30Sensor::getEco2Ppm() const {
  return _p->eco2;
}

uint32_t Sgp30Sensor::getTvocPpb() const {
  return _p->tvoc;
}

bool Sgp30Sensor::begin() {
  // 与 OLED/SHT30/GY-30 共用同一条 I2C；同参数重复 begin 无害
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);
  _p->present = _p->sgp.begin();
  _p->startMs = millis();
  _p->dataValid = false;
  _p->failStreak = 0;
  _p->lastScanMs = millis() - SGP30_RESCAN_MS;
  _p->lastReadMs = millis() - SGP30_READ_MS;

  if (_p->present) {
    Serial.println(F("[sgp] SGP30 已检测到（0x58），15s 暖机中（eCO2 为 TVOC 推算等效值）"));
  } else {
    Serial.println(F("[sgp] 未检测到 SGP30（检查接线），将周期重扫"));
  }
  return _p->present;
}

void Sgp30Sensor::handle() {
  // 1) 不在总线上：周期重扫（支持上电后才接线的热插拔自愈）
  if (!_p->present) {
    uint32_t now = millis();
    if (now - _p->lastScanMs >= SGP30_RESCAN_MS) {
      _p->lastScanMs = now;
      _p->present = _p->sgp.begin();
      if (_p->present) {
        _p->startMs = millis();   // 重新检测到视为重新上电，重新暖机
        Serial.println(F("[sgp] SGP30 重扫发现传感器，恢复采集"));
      }
    }
    return;
  }

  // 2) 在总线：按周期采集（暖机期内也读，但失败不计入故障）
  uint32_t now = millis();
  if (now - _p->lastReadMs < SGP30_READ_MS) return;
  _p->lastReadMs = now;

  if (!_p->sgp.IAQmeasure()) {
    if (!isWarmup()) {           // 暖机期失败是正常现象，不计故障
      _p->failStreak++;
      if (_p->failStreak >= SGP30_FAIL_STREAK) _p->dataValid = false;
      Serial.print(F("[sgp] 读取失败（连续 "));
      Serial.print(_p->failStreak);
      Serial.println(F(" 次）"));
    }
    return;
  }

  _p->eco2 = _p->sgp.eCO2;
  _p->tvoc = _p->sgp.TVOC;
  _p->failStreak = 0;
  _p->dataValid = true;
  Serial.print(F("[sgp] eCO2="));
  Serial.print(_p->eco2);
  Serial.print(F("ppm TVOC="));
  Serial.print(_p->tvoc);
  Serial.println(F("ppb"));
}
