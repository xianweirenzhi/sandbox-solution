#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>   // ESP8266 核心自带（EspSoftwareSerial），GY-39 UART 模式接收
#include <Adafruit_BME280.h>  // GY-39 直连模式：BME280 温/湿/气压（含补偿计算）

#include "net_config.h"   // PERIPH_I2C_SDA / PERIPH_I2C_SCL / PERIPH_GY39_UART_RX
#include "gy39_sensor.h"

// ===================== 采集参数（改地址/周期只动这里）=====================
#define GY39_I2C_ADDR    0x5B     // GY-39 I2C 模式 7 位地址（MCU_IIC，S0 焊桥接 GND）
#define GY39_ADDR_BME1   0x76     // 直连模式 BME280 地址①（SDO 接地）
#define GY39_ADDR_BME2   0x77     // 直连模式 BME280 地址②（SDO 接高）
#define GY39_ADDR_MAX    0x4A     // 直连模式 MAX44009 光照地址（固定）
#define GY39_READ_MS     2000UL   // 采集周期（毫秒；UART 帧约 1s 一发，按需取用）
#define GY39_FAIL_STREAK 3        // 连续失败 N 次判定传感器故障
#define GY39_RESCAN_MS   10000UL  // 未检测到传感器时的重扫周期（热插拔自愈）
#define GY39_REG_DATA    14       // I2C 数据寄存器长度（0x00~0x0D）
#define GY39_UART_BAUD   9600     // UART 模式波特率（手册默认；另一档 115200）
#define GY39_UART_ALIVE_MS   6000UL   // UART 帧流存活窗口（超此无有效帧计一次失败）
#define GY39_UART_DROP_MS  30000UL   // UART 帧流停止此时长降级回探测（支持换接线）

// ===================== 内部小工具 =====================

// 全总线扫描并打印应答的设备地址（仅在 I2C 探测失败时调用一次，辅助现场排障）
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
}

// ===================== 实现体 =====================
struct Gy39Sensor::Impl {
  // ---- 公共状态 ----
  uint8_t  mode = 0;             // 0=未检测到 1=I2C-MCU(0x5B) 2=UART 3=直连芯片(BME280+MAX44009)
  bool     thpValid = false;     // 温/湿/气压有效
  bool     luxValid = false;     // 光照有效
  float    temp = 0.0f;          // 最近一次有效温度（°C）
  float    hum = 0.0f;           // 最近一次有效湿度（%RH）
  unsigned long pressPa = 0;     // 最近一次有效大气压（Pa）
  float    lux = 0.0f;           // 最近一次有效光照（lux）
  uint8_t  failStreak = 0;       // 连续失败计数（成功清零）
  uint32_t lastReadMs = 0;       // 上次采集/健康检查时刻（限频用）
  uint32_t lastScanMs = 0;       // 上次重扫时刻（未在总线时用）

  // ---- I2C（0x5B）读事务三样式 ----
  uint8_t  style = 0;            // 0=A/B=C 记忆成功样式，见 readRegs
  uint8_t  verboseLeft = 6;      // 剩余详细失败打印配额（限噪）

  // ---- UART 帧解析状态机 ----
  SoftwareSerial uart;           // 仅收不发（TX 引脚 -1）
  uint8_t  uState = 0;           // 0=等5A① 1=等5A② 2=type 3=len 4=data 5=校验
  uint8_t  uType = 0;
  uint8_t  uLen = 0;
  uint8_t  uIdx = 0;
  uint8_t  uBuf[12];             // 数据域缓冲（0x45 帧 len=10）
  uint32_t lastFrameMs = 0;      // 最近一次校验通过帧的时刻

  // ---- 直连芯片模式（S1 焊桥 GND / 板边 I2C 焊盘，绕过模块 MCU）----
  Adafruit_BME280 bme;           // BME280 温/湿/气压（begin 时读校准参数）
  uint8_t  bmeAddr = 0;          // begin 成功的 BME280 地址（0x76/0x77；0=未上线）
  bool     maxOk = false;        // MAX44009 是否应答
  uint8_t  luxFail = 0;          // 光照路径连续失败计数（独立于温湿压路径）

  Impl() : uart(PERIPH_GY39_UART_RX, -1) {}

  // 大端读取：buf 起的 n 字节拼为无符号整数（n ≤ 4）
  static unsigned long beGet(const uint8_t *buf, uint8_t n) {
    unsigned long v = 0;
    for (uint8_t i = 0; i < n; i++) v = (v << 8) | buf[i];
    return v;
  }

  // 读数合理性（超物理范围视为异常，不污染缓存值）
  static bool sane(float t, float h, unsigned long pa) {
    return t >= -40.0f && t <= 85.0f && h >= 0.0f && h <= 100.0f &&
           pa >= 30000UL && pa <= 120000UL;
  }

  // 探测某 I2C 地址是否应答（地址 ACK 即认为在）
  static bool probeAddr(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
  }

  // ---- 直连模式初始化：BME280 两个候选地址依次 begin，MAX44009 探测 ----
  // MAX44009 上电默认即连续自动量程模式（800ms 周期），无需写配置即可读。
  // 返回是否任一芯片在线。
  bool startDirect() {
    bmeAddr = 0;
    if (bme.begin(GY39_ADDR_BME1, &Wire)) bmeAddr = GY39_ADDR_BME1;
    else if (bme.begin(GY39_ADDR_BME2, &Wire)) bmeAddr = GY39_ADDR_BME2;
    maxOk = probeAddr(GY39_ADDR_MAX);
    failStreak = 0;
    luxFail = 0;
    Serial.print(F("[gy39] GY-39 直连芯片模式（绕过模块 MCU）：BME280 "));
    Serial.print(bmeAddr == 0 ? F("未应答") : (bmeAddr == GY39_ADDR_BME1 ? F("0x76") : F("0x77")));
    Serial.print(F("，MAX44009 "));
    Serial.println(maxOk ? F("0x4A") : F("未应答"));
    return bmeAddr != 0 || maxOk;
  }

  // ---- MAX44009 读光照：读 0x03(高)/0x04(低)，lux = M·2^E·0.045 ----
  bool readMax(float &luxOut) {
    Wire.beginTransmission(GY39_ADDR_MAX);
    Wire.write(0x03);                          // 光照寄存器起始地址
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(GY39_ADDR_MAX, (uint8_t)2) != 2) return false;
    uint8_t hi = (uint8_t)Wire.read();
    uint8_t lo = (uint8_t)Wire.read();
    uint8_t e = hi >> 4;
    unsigned long m = ((unsigned long)(hi & 0x0F) << 4) | (lo & 0x0F);
    luxOut = (float)(m << e) * 0.045f;
    return luxOut <= 200000.0f;                // 超物理范围视为异常读
  }

  // ---- I2C 单个样式尝试：true = 14 字节全部到手 ----
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

  // I2C 读 0x00 起 14 字节数据寄存器；失败返回 false。
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

  // ---- UART：排空接收缓冲喂给帧状态机（每次 handle 调用）----
  void pumpUart() {
    while (uart.available() > 0) {
      uint8_t b = (uint8_t)uart.read();
      switch (uState) {
        case 0: if (b == 0x5A) uState = 1; break;
        case 1: if (b == 0x5A) uState = 2; else uState = 0; break;
        case 2: uType = b; uState = 3; break;
        case 3: uLen = b; uIdx = 0;
                uState = (uLen >= 1 && uLen <= sizeof(uBuf)) ? 4 : 0; break;
        case 4: uBuf[uIdx++] = b;
                if (uIdx == uLen) uState = 5; break;
        case 5: {  // 校验：帧头到数据最后一字节累加和低 8 位
                uint8_t sum = 0x5A + 0x5A + uType + uLen;
                for (uint8_t i = 0; i < uLen; i++) sum += uBuf[i];
                if (sum == b) onFrame();
                uState = 0;
                } break;
      }
    }
  }

  // 校验通过的帧分发（手册数据布局，大端 /100）
  void onFrame() {
    lastFrameMs = millis();
    if (uType == 0x15 && uLen >= 4) {            // 光照帧：lux 4B
      float l = beGet(uBuf, 4) / 100.0f;
      if (l <= 200000.0f) { lux = l; luxValid = true; }
    } else if (uType == 0x45 && uLen >= 10) {    // 气象帧：T2 P4 H2 Alt2(不用)
      float t = (float)(int16_t)beGet(uBuf, 2) / 100.0f;
      unsigned long pa = beGet(uBuf + 2, 4) / 100UL;
      float h = beGet(uBuf + 6, 2) / 100.0f;
      if (sane(t, h, pa)) { temp = t; hum = h; pressPa = pa; thpValid = true; }
    }
    // 未知类型帧：仅刷新存活时刻（校验通过即为有效帧流）
  }
};

// ===================== 公开接口 =====================

Gy39Sensor::Gy39Sensor() : _p(new Impl) {}

Gy39Sensor::~Gy39Sensor() {
  delete _p;
  _p = nullptr;
}

bool Gy39Sensor::isOk() const {
  return _p->mode != 0 && _p->failStreak < GY39_FAIL_STREAK;
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
  // GY-39 的 MCU 从机被读寄存器时可能拉住 SCL（时钟拉伸）；ESP8266 核心默认
  // 只容忍 230µs，超时整笔判失败（地址探测事务短反而能过）。放宽到 3ms，
  // 对 OLED/SGP30 等不拉伸的设备无影响（仅为超时上限，不是主动等待）。
  Wire.setClockStretchLimit(3000);

  // UART 常开监听（出厂默认模式免焊接线即可用；与 I2C 探测并行自适应）
  _p->uart.begin(GY39_UART_BAUD);

  // 让 handle 首轮即可进入扫描/采集节奏
  _p->lastScanMs = millis() - GY39_RESCAN_MS;
  _p->lastReadMs = millis() - GY39_READ_MS;

  if (_p->probeAddr(GY39_I2C_ADDR)) {
    _p->mode = 1;
    Serial.println(F("[gy39] GY-39 已检测到（I2C 0x5B 模式）"));
  } else if (_p->startDirect()) {
    _p->mode = 3;   // 直连芯片（BME280+MAX44009 已在总线）
  } else {
    Serial.println(F("[gy39] 未检测到 GY-39，将周期重扫 + 并行监听 UART（GPIO14）"));
    Serial.println(F("[gy39] 接线三选一，固件自适应："));
    Serial.println(F("[gy39]  ① 直连芯片（推荐）：模块板边 SDA/SCL 焊盘→板上 SDA/SCL"));
    Serial.println(F("[gy39]  ② UART 免焊（出厂默认）：CT→GPIO14，DR 不接"));
    Serial.println(F("[gy39]  ③ I2C-MCU（S0 焊桥 GND）：CT→SCL、DR→SDA（0x5B）"));
    scanBus();   // 打印总线上实际应答的设备，辅助定位
  }
  return true;
}

void Gy39Sensor::handle() {
  uint32_t now = millis();

  if (_p->mode == 0) {
    // ---- 未上线：UART 帧流提升 + 周期 I2C/直连重扫，哪边先来用哪边 ----
    _p->pumpUart();
    if (_p->lastFrameMs != 0 && now - _p->lastFrameMs < GY39_UART_ALIVE_MS) {
      _p->mode = 2;
      _p->failStreak = 0;
      Serial.println(F("[gy39] 检测到 GY-39 UART 帧流（CT→GPIO14），采用 UART 模式"));
      return;
    }
    if (now - _p->lastScanMs >= GY39_RESCAN_MS) {
      _p->lastScanMs = now;
      if (_p->probeAddr(GY39_I2C_ADDR)) {
        _p->mode = 1;
        _p->failStreak = 0;
        Serial.println(F("[gy39] 重扫发现 GY-39（I2C 0x5B），恢复采集"));
      } else if (_p->startDirect()) {
        _p->mode = 3;   // 直连芯片上总线（热插拔/换接线）
      }
    }
    return;
  }

  if (_p->mode == 3) {
    // ---- 直连模式：BME280 + MAX44009 周期采集；双芯片均消失则回探测 ----
    if (now - _p->lastReadMs < GY39_READ_MS) return;
    _p->lastReadMs = now;

    bool bmeAlive = _p->bmeAddr != 0 && Impl::probeAddr(_p->bmeAddr);
    bool maxAlive = _p->maxOk && Impl::probeAddr(GY39_ADDR_MAX);
    if (!bmeAlive && !maxAlive) {
      _p->mode = 0;
      _p->thpValid = _p->luxValid = false;
      _p->lastScanMs = now;
      Serial.println(F("[gy39] 直连芯片均无应答，回到探测（支持热插拔）"));
      return;
    }

    // 温/湿/气压（BME280，Adafruit 库含出厂校准补偿）
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
    return;
  }

  if (_p->mode == 2) {
    // ---- UART 模式：持续解析；按周期做健康检查 ----
    _p->pumpUart();
    if (now - _p->lastFrameMs >= GY39_UART_DROP_MS) {
      // 帧流停止过久（换接线/拔线）：降级回探测，支持热切换
      _p->mode = 0;
      _p->thpValid = _p->luxValid = false;
      _p->lastScanMs = now;
      _p->lastFrameMs = 0;
      Serial.println(F("[gy39] UART 帧流超时，回到探测（I2C 重扫 + UART 监听）"));
      return;
    }
    if (now - _p->lastReadMs < GY39_READ_MS) return;
    _p->lastReadMs = now;
    if (now - _p->lastFrameMs < GY39_UART_ALIVE_MS) {
      if (_p->failStreak) Serial.println(F("[gy39] UART 帧流恢复"));
      _p->failStreak = 0;
    } else {
      _p->failStreak++;
      if (_p->failStreak >= GY39_FAIL_STREAK) { _p->thpValid = _p->luxValid = false; }
      Serial.print(F("[gy39] UART 无有效帧（连续 "));
      Serial.print(_p->failStreak);
      Serial.println(F(" 个检查周期）"));
    }
    return;
  }

  // ---- I2C 模式：按周期采集 ----
  if (now - _p->lastReadMs < GY39_READ_MS) return;
  _p->lastReadMs = now;

  uint8_t r[GY39_REG_DATA];
  if (!_p->readRegs(r)) {
    _p->failStreak++;
    if (_p->failStreak >= GY39_FAIL_STREAK) { _p->thpValid = _p->luxValid = false; }
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

  if (!Impl::sane(t, hum, pa) || lux > 200000.0f) {
    _p->failStreak++;
    if (_p->failStreak >= GY39_FAIL_STREAK) { _p->thpValid = _p->luxValid = false; }
    Serial.println(F("[gy39] 读数超物理范围，按失败处理"));
    return;
  }

  _p->temp = t;
  _p->hum = hum;
  _p->pressPa = pa;
  _p->lux = lux;
  _p->failStreak = 0;
  _p->thpValid = _p->luxValid = true;
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
