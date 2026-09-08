#include <Arduino.h>
#include <Wire.h>

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "gy39_sensor.h"

// ===================== 采集参数（改地址/周期只动这里）=====================
#define GY39_I2C_ADDR    0x5B     // GY-39 I2C 模式 7 位地址（MCU_IIC，S0 焊桥接 GND）
#define GY39_READ_MS     2000UL   // 采集周期（毫秒；模块内部 ~10Hz 刷新，只管按需读）
#define GY39_FAIL_STREAK 3        // 连续失败 N 次判定传感器故障
#define GY39_RESCAN_MS   10000UL  // 未检测到传感器时的重扫周期（热插拔自愈）
#define GY39_REG_DATA    14       // 数据寄存器长度（0x00~0x0D）

// ===================== 实现体 =====================
struct Gy39Sensor::Impl {
  bool     present = false;    // 总线上是否检测到模块
  bool     dataValid = false;  // 是否有可信读数
  float    temp = 0.0f;        // 最近一次有效温度（°C）
  float    hum = 0.0f;         // 最近一次有效湿度（%RH）
  unsigned long pressPa = 0;   // 最近一次有效大气压（Pa）
  float    lux = 0.0f;         // 最近一次有效光照（lux）
  uint8_t  failStreak = 0;     // 连续读取失败计数（成功清零）
  uint32_t lastReadMs = 0;     // 上次采集时刻（限频用）
  uint32_t lastScanMs = 0;     // 上次重扫时刻（未在总线时用）

  // 大端读取：buf 起的 n 字节拼为无符号整数（n ≤ 4）
  static unsigned long beGet(const uint8_t *buf, uint8_t n) {
    unsigned long v = 0;
    for (uint8_t i = 0; i < n; i++) v = (v << 8) | buf[i];
    return v;
  }

  // 探测总线上是否有模块（地址 ACK 即认为在）
  bool probe() {
    Wire.beginTransmission(GY39_I2C_ADDR);
    return Wire.endTransmission() == 0;
  }

  // 读 0x00 起 14 字节数据寄存器；失败返回 false
  bool readRegs(uint8_t *out) {
    Wire.beginTransmission(GY39_I2C_ADDR);
    Wire.write(0x00);                        // 起始寄存器地址
    if (Wire.endTransmission(false) != 0) return false;   // repeated start
    if (Wire.requestFrom(GY39_I2C_ADDR, (uint8_t)GY39_REG_DATA) != GY39_REG_DATA) {
      return false;
    }
    for (uint8_t i = 0; i < GY39_REG_DATA; i++) {
      if (!Wire.available()) return false;
      out[i] = (uint8_t)Wire.read();
    }
    return true;
  }
};

// ===================== 公开接口 =====================

Gy39Sensor::Gy39Sensor() : _p(new Impl) {}

Gy39Sensor::~Gy39Sensor() {
  delete _p;
  _p = nullptr;
}

bool Gy39Sensor::isOk() const {
  return _p->present && _p->failStreak < GY39_FAIL_STREAK;
}

bool Gy39Sensor::hasData() const {
  return _p->dataValid;
}

float Gy39Sensor::getTempC() const {
  return _p->temp;
}

float Gy39Sensor::getHumRH() const {
  return _p->hum;
}

unsigned long Gy39Sensor::getPressPa() const {
  return _p->pressPa;
}

float Gy39Sensor::getLux() const {
  return _p->lux;
}

bool Gy39Sensor::begin() {
  // 与 OLED 共用同一条 I2C；同参数重复 begin 无害（各自模块独立初始化总线）
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);
  _p->present = _p->probe();
  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - GY39_RESCAN_MS;
  _p->lastReadMs = millis() - GY39_READ_MS;

  if (_p->present) {
    Serial.println(F("[gy39] GY-39 已检测到（0x5B，I2C 模式）"));
  } else {
    Serial.println(F("[gy39] 未检测到 GY-39（检查接线/地址），将周期重扫"));
    Serial.println(F("[gy39] 提示：模块须为 I2C 模式（S0 焊桥接 GND），CT=SCL、DR=SDA"));
  }
  return _p->present;
}

void Gy39Sensor::handle() {
  // 1) 不在总线上：周期重扫（支持上电后才接线的热插拔自愈）
  if (!_p->present) {
    uint32_t now = millis();
    if (now - _p->lastScanMs >= GY39_RESCAN_MS) {
      _p->lastScanMs = now;
      _p->present = _p->probe();
      if (_p->present) {
        _p->failStreak = 0;
        Serial.println(F("[gy39] GY-39 重扫发现模块，恢复采集"));
      }
    }
    return;
  }

  // 2) 在总线：按周期采集
  uint32_t now = millis();
  if (now - _p->lastReadMs < GY39_READ_MS) return;
  _p->lastReadMs = now;

  uint8_t r[GY39_REG_DATA];
  if (!_p->readRegs(r)) {
    _p->failStreak++;
    if (_p->failStreak >= GY39_FAIL_STREAK) _p->dataValid = false;
    Serial.print(F("[gy39] 读取失败（连续 "));
    Serial.print(_p->failStreak);
    Serial.println(F(" 次）"));
    return;
  }

  // 解析（官方手册寄存器表，大端，/100）：
  //   0x00~0x03 lux | 0x04~0x05 T(有符号) | 0x06~0x09 P | 0x0A~0x0B HUM | 0x0C~ 海拔(不用)
  float         t   = (float)(int16_t)Impl::beGet(r + 4, 2) / 100.0f;
  unsigned long pa  = Impl::beGet(r + 6, 4) / 100UL;
  float         hum = Impl::beGet(r + 10, 2) / 100.0f;
  float         lux = Impl::beGet(r + 0, 4) / 100.0f;

  // 读数合理性：超物理范围视为总线毛刺/异常帧（计失败，不污染缓存值）
  if (t < -40.0f || t > 85.0f || hum < 0.0f || hum > 100.0f ||
      pa < 30000UL || pa > 120000UL || lux < 0.0f || lux > 200000.0f) {
    _p->failStreak++;
    if (_p->failStreak >= GY39_FAIL_STREAK) _p->dataValid = false;
    Serial.println(F("[gy39] 读数超物理范围，按失败处理"));
    return;
  }

  _p->temp = t;
  _p->hum = hum;
  _p->pressPa = pa;
  _p->lux = lux;
  _p->failStreak = 0;
  _p->dataValid = true;
  Serial.print(F("[gy39] T="));
  Serial.print(t, 1);
  Serial.print(F("C H="));
  Serial.print(hum, 1);
  Serial.print(F("% P="));
  Serial.print((unsigned)(pa / 100UL));
  Serial.print(F("hPa L="));
  Serial.print(lux, 0);
  Serial.println(F("lux"));
}
