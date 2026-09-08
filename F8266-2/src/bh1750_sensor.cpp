#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "bh1750_sensor.h"

// ===================== 采集参数（改地址/周期只动这里）=====================
#define BH1750_I2C_ADDR    0x23    // GY-30 默认地址（ADDR 引脚接高则为 0x5C）
#define BH1750_READ_MS     2000UL  // 采集周期（毫秒；高分辨率模式单次测量约 120ms）
#define BH1750_FAIL_STREAK 3       // 连续失败 N 次判定传感器故障
#define BH1750_RESCAN_MS   10000UL // 未检测到传感器时的重扫周期（热插拔自愈）

// ===================== 实现体 =====================
struct Bh1750Sensor::Impl {
  BH1750   meter = BH1750();
  bool     present = false;    // 总线上是否检测到传感器
  bool     dataValid = false;  // 是否有可信读数
  float    lux = 0.0f;         // 最近一次有效光照（lux）
  uint8_t  failStreak = 0;     // 连续读取失败计数（成功清零）
  uint32_t lastReadMs = 0;     // 上次采集时刻（限频用）
  uint32_t lastScanMs = 0;     // 上次重扫时刻（未在总线时用）
};

// ===================== 公开接口 =====================

Bh1750Sensor::Bh1750Sensor() : _p(new Impl) {}

Bh1750Sensor::~Bh1750Sensor() {
  delete _p;
  _p = nullptr;
}

bool Bh1750Sensor::isOk() const {
  return _p->present && _p->failStreak < BH1750_FAIL_STREAK;
}

bool Bh1750Sensor::hasData() const {
  return _p->dataValid;
}

float Bh1750Sensor::getLux() const {
  return _p->lux;
}

bool Bh1750Sensor::begin() {
  // 与 OLED/SHT30 共用同一条 I2C；同参数重复 begin 无害（各自模块独立初始化总线）
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);
  _p->present = _p->meter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_I2C_ADDR);
  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - BH1750_RESCAN_MS;
  _p->lastReadMs = millis() - BH1750_READ_MS;

  if (_p->present) {
    Serial.println(F("[gy30] GY-30(BH1750) 已检测到（0x23）"));
  } else {
    Serial.println(F("[gy30] 未检测到 GY-30（检查接线/地址），将周期重扫"));
  }
  return _p->present;
}

void Bh1750Sensor::handle() {
  // 1) 不在总线上：周期重扫（支持上电后才接线的热插拔自愈）
  if (!_p->present) {
    uint32_t now = millis();
    if (now - _p->lastScanMs >= BH1750_RESCAN_MS) {
      _p->lastScanMs = now;
      _p->present = _p->meter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_I2C_ADDR);
      if (_p->present) {
        Serial.println(F("[gy30] GY-30 重扫发现传感器，恢复采集"));
      }
    }
    return;
  }

  // 2) 在总线：按周期采集（readLightLevel 失败返回负值/NAN）
  uint32_t now = millis();
  if (now - _p->lastReadMs < BH1750_READ_MS) return;
  _p->lastReadMs = now;

  float l = _p->meter.readLightLevel();
  if (isnan(l) || l < 0) {
    _p->failStreak++;
    if (_p->failStreak >= BH1750_FAIL_STREAK) _p->dataValid = false;
    Serial.print(F("[gy30] 读取失败（连续 "));
    Serial.print(_p->failStreak);
    Serial.println(F(" 次）"));
    return;
  }

  _p->lux = l;
  _p->failStreak = 0;
  _p->dataValid = true;
  Serial.print(F("[gy30] L="));
  Serial.print(l, 0);
  Serial.println(F("lx"));
}
