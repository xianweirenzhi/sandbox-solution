#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>  // BME280 温/湿/气压（含出厂校准补偿）

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "gy39_sensor.h"

// ===================== 采集参数（改地址/阈值/周期只动这里）=====================
#define GY39_ADDR_BME1   0x76     // BME280 地址①（SDO 接地）
#define GY39_ADDR_BME2   0x77     // BME280 地址②（SDO 接高）
#define GY39_ADDR_MAX    0x4A     // MAX44009 光照地址（固定）
#define GY39_READ_MS     2000UL   // 采集周期（毫秒）
#define GY39_FAIL_STREAK 3        // 连续失败 N 次判定传感器故障
#define GY39_RESCAN_MS   10000UL  // 未检测到传感器时的重扫周期（热插拔自愈）
// ★光照现场标定倍率：本模块 MAX44009 实测读数恒偏低约 100 倍（环境 5↔BH1750 约
// 500、手电直射 150↔约 1.5 万，两组比例一致，判定为兼容片灵敏度/模块窗口衰减的
// 固定倍率偏差）。精调方法：与 F8266-1(BH1750) 并排同照度对照，倍率 = 参考值/本机原始值。
#define GY39_LUX_CAL     100.0f

// ===================== 实现体 =====================
struct Gy39Sensor::Impl {
  bool     present = false;    // 任一芯片在线
  bool     thpValid = false;   // 温/湿/气压有效
  bool     luxValid = false;   // 光照有效
  float    temp = 0.0f;        // 最近一次有效温度（°C）
  float    hum = 0.0f;         // 最近一次有效湿度（%RH）
  unsigned long pressPa = 0;   // 最近一次有效大气压（Pa）
  float    lux = 0.0f;         // 最近一次有效光照（lux，已乘标定倍率）
  uint8_t  failStreak = 0;     // 温湿压路径连续失败计数（成功清零）
  uint8_t  luxFail = 0;        // 光照路径连续失败计数（独立，互不拖累）
  uint32_t lastReadMs = 0;     // 上次采集时刻（限频用）
  uint32_t lastScanMs = 0;     // 上次重扫时刻（未在线时用）
  uint8_t  maxVerboseLeft = 10;  // MAX44009 原始字节打印配额（现场标定排查用）

  // ---- BME280（Adafruit 库，begin 时读出厂校准参数）----
  Adafruit_BME280 bme;
  uint8_t  bmeAddr = 0;        // begin 成功的地址（0x76/0x77；0=未上线）
  bool     maxOk = false;      // MAX44009 是否应答

  // 读数合理性（超物理范围视为异常，不污染缓存值）
  static bool sane(float t, float h, unsigned long pa) {
    return t >= -40.0f && t <= 85.0f && h >= 0.0f && h <= 100.0f &&
           pa >= 30000UL && pa <= 120000UL;
  }

  // 探测某 I2C 地址是否应答
  static bool probeAddr(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
  }

  // 单字节寄存器读（★该 MAX44009 连读 2 字节返回同一寄存器重复值——
  // 寄存器指针不自动递增，必须两次独立单字节读）
  static bool readReg8(uint8_t addr, uint8_t reg, uint8_t &out) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
    out = (uint8_t)Wire.read();
    return true;
  }

  // 探测并初始化：BME280 两个候选地址依次 begin，MAX44009 探测。
  // MAX44009 上电默认即连续自动量程模式，无需写配置即可读。
  // 返回是否任一芯片在线。
  bool startDirect() {
    bmeAddr = 0;
    if (bme.begin(GY39_ADDR_BME1, &Wire)) bmeAddr = GY39_ADDR_BME1;
    else if (bme.begin(GY39_ADDR_BME2, &Wire)) bmeAddr = GY39_ADDR_BME2;
    maxOk = probeAddr(GY39_ADDR_MAX);
    failStreak = 0;
    luxFail = 0;
    if (bmeAddr != 0 || maxOk) {
      Serial.print(F("[gy39] GY-39 直连芯片：BME280 "));
      Serial.print(bmeAddr == 0 ? F("未应答") : (bmeAddr == GY39_ADDR_BME1 ? F("0x76") : F("0x77")));
      Serial.print(F("，MAX44009 "));
      Serial.println(maxOk ? F("0x4A") : F("未应答"));
      return true;
    }
    return false;
  }

  // MAX44009 读光照：0x03=E[3:0]M[11:8]、0x04=M[7:4]0000，lux = M·2^E·0.045×标定
  bool readMax(float &luxOut) {
    uint8_t hi, lo;
    if (!readReg8(GY39_ADDR_MAX, 0x03, hi)) return false;
    if (!readReg8(GY39_ADDR_MAX, 0x04, lo)) return false;
    uint8_t e = hi >> 4;
    unsigned long m = ((unsigned long)(hi & 0x0F) << 4) | (lo >> 4);
    luxOut = (float)(m << e) * 0.045f * GY39_LUX_CAL;
    if (maxVerboseLeft) {
      maxVerboseLeft--;
      Serial.print(F("[gy39] MAX44009 raw: hi=0x"));
      if (hi < 16) Serial.print('0');
      Serial.print(hi, HEX);
      Serial.print(F(" lo=0x"));
      if (lo < 16) Serial.print('0');
      Serial.print(lo, HEX);
      Serial.print(F(" -> "));
      Serial.print(luxOut, 0);
      Serial.println(F("lux(cal)"));
    }
    return luxOut <= 200000.0f;                // 超物理范围视为异常读
  }
};

// ===================== 内部小工具 =====================

// 全总线扫描并打印应答的设备地址（仅在探测失败时调用一次，辅助现场排障）
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
      else if (a == 0x4A) Serial.print(F("(MAX44009)"));
      else if (a == 0x76 || a == 0x77) Serial.print(F("(BME280)"));
      Serial.print(' ');
      found++;
    }
  }
  if (found == 0) Serial.println(F("无设备应答（查 VCC/GND/SDA/SCL）"));
  else Serial.println();
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
  return _p->thpValid;
}

bool Gy39Sensor::hasLux() const {
  return _p->luxValid;
}

float Gy39Sensor::getTempC() const { return _p->temp; }
float Gy39Sensor::getHumRH() const { return _p->hum; }
unsigned long Gy39Sensor::getPressPa() const { return _p->pressPa; }
float Gy39Sensor::getLux() const { return _p->lux; }

bool Gy39Sensor::begin() {
  // 与 OLED 共用同一条 I2C；同参数重复 begin 无害（各自模块独立初始化总线）
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);

  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - GY39_RESCAN_MS;
  _p->lastReadMs = millis() - GY39_READ_MS;

  if (_p->startDirect()) {
    _p->present = true;
  } else {
    _p->present = false;
    Serial.println(F("[gy39] 未检测到 GY-39 直连芯片，将周期重扫"));
    Serial.println(F("[gy39] 接线：模块板边 SDA/SCL 焊盘→板上 SDA/SCL"));
    Serial.println(F("[gy39]       （BME280 0x76/0x77 + MAX44009 0x4A，VCC→3V3、GND→GND）"));
    scanBus();   // 打印总线上实际应答的设备，辅助定位
  }
  return _p->present;
}

void Gy39Sensor::handle() {
  uint32_t now = millis();

  // 1) 未在线：周期重扫（支持上电后才接线的热插拔自愈）
  if (!_p->present) {
    if (now - _p->lastScanMs >= GY39_RESCAN_MS) {
      _p->lastScanMs = now;
      if (_p->startDirect()) {
        _p->present = true;
        Serial.println(F("[gy39] 重扫发现 GY-39 直连芯片，恢复采集"));
      }
    }
    return;
  }

  // 2) 在线：按周期采集
  if (now - _p->lastReadMs < GY39_READ_MS) return;
  _p->lastReadMs = now;

  bool bmeAlive = _p->bmeAddr != 0 && Impl::probeAddr(_p->bmeAddr);
  bool maxAlive = _p->maxOk && Impl::probeAddr(GY39_ADDR_MAX);
  if (!bmeAlive && !maxAlive) {
    _p->present = false;                       // 双芯片均消失：回探测（热插拔）
    _p->thpValid = _p->luxValid = false;
    _p->lastScanMs = now;
    Serial.println(F("[gy39] 直连芯片均无应答，回到探测"));
    return;
  }

  // 温/湿/气压（BME280）
  if (bmeAlive) {
    float t = _p->bme.readTemperature();
    float h = _p->bme.readHumidity();
    float pa = _p->bme.readPressure();
    if (isnan(t) || isnan(h) || isnan(pa) ||
        !Impl::sane(t, h, (unsigned long)pa)) {
      _p->failStreak++;
      if (_p->failStreak >= GY39_FAIL_STREAK) _p->thpValid = false;
      Serial.println(F("[gy39] BME280 读取失败/超物理范围"));
    } else {
      _p->temp = t;
      _p->hum = h;
      _p->pressPa = (unsigned long)pa;
      _p->failStreak = 0;
      _p->thpValid = true;
    }
  }

  // 光照（MAX44009，独立失败计数，不影响温湿压）
  if (maxAlive) {
    float l;
    if (_p->readMax(l)) {
      _p->lux = l;
      _p->luxValid = true;
      _p->luxFail = 0;
    } else if (++_p->luxFail >= GY39_FAIL_STREAK) {
      _p->luxValid = false;
    }
  }

  if (_p->thpValid || _p->luxValid) {
    Serial.print(F("[gy39] "));
    if (_p->thpValid) {
      Serial.print(F("T="));
      Serial.print(_p->temp, 1);
      Serial.print(F("C H="));
      Serial.print(_p->hum, 1);
      Serial.print(F("% P="));
      Serial.print((unsigned)(_p->pressPa / 100UL));
      Serial.print(F("hPa "));
    }
    if (_p->luxValid) {
      Serial.print(F("L="));
      Serial.print(_p->lux, 0);
      Serial.print(F("lux"));
    }
    Serial.println();
  }
}
