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
  uint8_t  style = 0;          // 读事务样式（0=A/B=C 记忆成功样式，见 readRegs）
  uint8_t  verboseLeft = 6;    // 剩余详细失败打印配额（限噪）
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

  // 单个样式尝试：true = 14 字节全部到手
  // rc（endTransmission 返回码）：2=NACK 地址 / 3=NACK 数据 / 4=其他（含时钟拉伸超时）
  bool tryStyle(uint8_t s, uint8_t *out) {
    if (s != 2) {                              // 样式 A/B：先写寄存器指针 0x00
      Wire.beginTransmission(GY39_I2C_ADDR);
      Wire.write(0x00);
      uint8_t rc = Wire.endTransmission(s == 0);
      if (rc != 0) {
        if (verboseLeft) { verboseLeft--;
          Serial.print(F("[gy39] 样式")); Serial.print((char)('A' + s));
          Serial.print(F(" 写指针失败 rc=")); Serial.println(rc); }
        return false;
      }
    }
    uint8_t got = Wire.requestFrom(GY39_I2C_ADDR, (uint8_t)GY39_REG_DATA);
    if (got != GY39_REG_DATA) {
      if (verboseLeft) { verboseLeft--;
        Serial.print(F("[gy39] 样式")); Serial.print((char)('A' + s));
        Serial.print(F(" 读回不足 rc=255 got=")); Serial.println(got); }
      return false;
    }
    for (uint8_t i = 0; i < GY39_REG_DATA; i++) {
      if (!Wire.available()) return false;
      out[i] = (uint8_t)Wire.read();
    }
    return true;
  }

  // 读 0x00 起 14 字节数据寄存器；失败返回 false。
  // 模块 MCU 的 I2C 从机实现存在个体差异，按 A→B→C 轮换尝试并记住成功样式：
  //   A = 写指针 + repeated start（标准读法）
  //   B = 写指针 + STOP 再读（部分固件不支持 repeated start）
  //   C = 不写指针直接读（从 0x00 起顺序流出）
  bool readRegs(uint8_t *out) {
    for (uint8_t t = 0; t < 3; t++) {
      uint8_t s = (style + t) % 3;
      if (tryStyle(s, out)) {
        if (s != style) {
          style = s;
          Serial.print(F("[gy39] 采用读事务样式 "));
          Serial.println((char)('A' + s));
        }
        return true;
      }
    }
    return false;
  }
};

// ===================== 内部小工具 =====================

// 全总线扫描并打印应答的设备地址（仅在 GY-39 探测失败时调用一次，辅助现场排障）
static void scanBus() {
  Serial.print(F("[gy39] I2C 总线扫描："));
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("0x"));
      Serial.print(a, HEX);
      // 已知地址标注归属，其余打印原始地址
      if (a == 0x3C)      Serial.print(F("(OLED)"));
      else if (a == 0x58) Serial.print(F("(SGP30)"));
      else if (a == 0x5B) Serial.print(F("(GY39)"));
      else if (a == 0x4A) Serial.print(F("(MAX44009?)"));
      else if (a == 0x76 || a == 0x77) Serial.print(F("(BME280?)"));
      Serial.print(' ');
      found++;
    }
  }
  if (found == 0) Serial.println(F("无设备应答（查 VCC/GND/SDA/SCL）"));
  else Serial.println();
  // 结果解读：见 0x5B → 模块已在总线（另查寄存器读取）；见 0x4A/0x76/0x77 →
  // S1 被接 GND（直连芯片模式），0x5B 不会出现，应恢复 S1 默认；只有 OLED/SGP30 →
  // GY-39 未应答，多为 S0 未接 GND（UART 模式）或 CT/DR 接反。
}

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
  // GY-39 的 MCU 从机被读寄存器时可能拉住 SCL（时钟拉伸）；ESP8266 核心默认
  // 只容忍 230µs，超时整笔判失败（地址探测事务短反而能过）。放宽到 3ms，
  // 对 OLED/SGP30 等不拉伸的设备无影响（仅为超时上限，不是主动等待）。
  Wire.setClockStretchLimit(3000);
  _p->present = _p->probe();
  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - GY39_RESCAN_MS;
  _p->lastReadMs = millis() - GY39_READ_MS;

  if (_p->present) {
    Serial.println(F("[gy39] GY-39 已检测到（0x5B，I2C 模式）"));
  } else {
    Serial.println(F("[gy39] 未检测到 GY-39（检查接线/地址），将周期重扫"));
    Serial.println(F("[gy39] 提示：模块须为 I2C 模式（S0 焊桥接 GND），CT=SCL、DR=SDA"));
    scanBus();   // 打印总线上实际应答的设备，辅助定位（S0 模式/接线/接反）
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
