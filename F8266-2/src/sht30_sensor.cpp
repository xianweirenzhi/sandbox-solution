#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "sht30_sensor.h"

// ===================== 采集参数（改地址/周期只动这里）=====================
#define SHT30_I2C_ADDR    0x44     // SHT30 默认地址（ADDR 引脚接高则为 0x45）
#define SHT30_READ_MS     2000UL   // 采集周期（毫秒）
#define SHT30_FAIL_STREAK 3        // 连续失败 N 次判定传感器故障
#define SHT30_RESCAN_MS   10000UL  // 未检测到传感器时的重扫周期（热插拔自愈）

// ===================== 实现体 =====================
struct Sht30Sensor::Impl {
  Adafruit_SHT31 sht = Adafruit_SHT31();
  bool     present = false;    // 总线上是否检测到传感器
  bool     dataValid = false;  // 是否有可信读数
  float    temp = 0.0f;        // 最近一次有效温度（°C）
  float    hum = 0.0f;         // 最近一次有效湿度（%RH）
  uint8_t  failStreak = 0;     // 连续读取失败计数（成功清零）
  uint32_t lastReadMs = 0;     // 上次采集时刻（限频用）
  uint32_t lastScanMs = 0;     // 上次重扫时刻（未在总线时用）
};

// ===================== 公开接口 =====================

Sht30Sensor::Sht30Sensor() : _p(new Impl) {}

Sht30Sensor::~Sht30Sensor() {
  delete _p;
  _p = nullptr;
}

bool Sht30Sensor::isOk() const {
  return _p->present && _p->failStreak < SHT30_FAIL_STREAK;
}

bool Sht30Sensor::hasData() const {
  return _p->dataValid;
}

float Sht30Sensor::getTempC() const {
  return _p->temp;
}

float Sht30Sensor::getHumRH() const {
  return _p->hum;
}

bool Sht30Sensor::begin() {
  // 与 OLED 共用同一条 I2C；同参数重复 begin 无害（各自模块独立初始化总线）
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);
  _p->present = _p->sht.begin(SHT30_I2C_ADDR);
  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - SHT30_RESCAN_MS;
  _p->lastReadMs = millis() - SHT30_READ_MS;

  if (_p->present) {
    Serial.println(F("[sht] SHT30 已检测到（0x44）"));
  } else {
    Serial.println(F("[sht] 未检测到 SHT30（检查接线/地址），将周期重扫"));
  }
  return _p->present;
}

void Sht30Sensor::handle() {
  // 1) 不在总线上：周期重扫（支持上电后才接线的热插拔自愈）
  if (!_p->present) {
    uint32_t now = millis();
    if (now - _p->lastScanMs >= SHT30_RESCAN_MS) {
      _p->lastScanMs = now;
      _p->present = _p->sht.begin(SHT30_I2C_ADDR);
      if (_p->present) {
        Serial.println(F("[sht] SHT30 重扫发现传感器，恢复采集"));
      }
    }
    return;
  }

  // 2) 在总线：按周期采集
  uint32_t now = millis();
  if (now - _p->lastReadMs < SHT30_READ_MS) return;
  _p->lastReadMs = now;

  // 读数返回 NAN 表示通信/校验失败（库内建 CRC 校验，坏数据不会混入）
  float t = _p->sht.readTemperature();
  float h = _p->sht.readHumidity();
  if (isnan(t) || isnan(h)) {
    _p->failStreak++;
    if (_p->failStreak >= SHT30_FAIL_STREAK) _p->dataValid = false;
    Serial.print(F("[sht] 读取失败（连续 "));
    Serial.print(_p->failStreak);
    Serial.println(F(" 次）"));
    return;
  }

  _p->temp = t;
  _p->hum = h;
  _p->failStreak = 0;
  _p->dataValid = true;
  Serial.print(F("[sht] T="));
  Serial.print(t, 1);
  Serial.print(F("C H="));
  Serial.print(h, 1);
  Serial.println(F("%"));
}
