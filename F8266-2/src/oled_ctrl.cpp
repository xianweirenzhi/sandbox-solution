#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "net_config.h"   // NET_DEVICE_NAME / PERIPH_I2C_SDA / PERIPH_I2C_SCL
#include "oled_ctrl.h"

// ===================== 显示参数（改地址只动这里）=====================
// I2C 引脚集中定义在 net_config.h（与 SHT30/GY-30/SGP30 共线）；0.96" SSD1306 常见地址 0x3C（部分模组 0x3D）
#define OLED_I2C_ADDR  0x3C
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_INFO_ROWS 6    // 信息区行数（标题区压缩为 1 行后，8 行屏余 7 行取 6）
#define OLED_INFO_Y    8    // 信息区首行纵坐标（标题行之后，单位像素）

// ===================== 实现体 =====================
struct OledCtrl::Impl {
  Adafruit_SSD1306 display = Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire);
  bool   ok = false;        // 屏是否初始化成功
  bool   hasInfo[OLED_INFO_ROWS] = {false};  // 各信息行是否已写入
  String infoLine[OLED_INFO_ROWS];           // 各信息行内容（变化才重绘）
};

// ===================== 公开接口 =====================

OledCtrl::OledCtrl() : _p(new Impl) {}

OledCtrl::~OledCtrl() {
  delete _p;
  _p = nullptr;
}

bool OledCtrl::isOk() const {
  return _p->ok;
}

void OledCtrl::setInfoLine(uint8_t row, const String &text) {
  if (!_p->ok || row >= OLED_INFO_ROWS) return;  // 屏不在/行号越界则丢弃
  if (_p->hasInfo[row] && text == _p->infoLine[row]) return;  // 内容未变不重绘（省 I2C 流量）
  _p->infoLine[row] = text;
  _p->hasInfo[row] = text.length() > 0;
  render();
}

bool OledCtrl::begin() {
  Wire.begin(PERIPH_I2C_SDA, PERIPH_I2C_SCL);

  if (!_p->display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    _p->ok = false;
    Serial.println(F("[oled] 未检测到 SSD1306（检查 I2C 接线 / 地址 0x3C/0x3D）"));
    return false;
  }
  _p->ok = true;
  render();

  Serial.println(F("[oled] SSD1306 已激活"));
  return true;
}

void OledCtrl::handle() {
  // 预留：后续整页子系统状态（多行/分区）在本函数按周期重绘。
  // 当前信息行由 setInfoLine() 按需驱动，此处无周期内容。
}

// ===================== 私有实现 =====================

void OledCtrl::render() {
  // 标题行：设备名 + 项目名合并一行（激活验证期单独的 "OLED OK" 行已退役，让位给子系统）
  // 信息区：各子系统写入的状态文本（如 "T:25.3C H:56%" / "C:800ppm V:12ppb"），未写入的行留空
  _p->display.clearDisplay();
  _p->display.setTextSize(1);
  _p->display.setTextColor(SSD1306_WHITE);
  _p->display.setCursor(0, 0);
  _p->display.print(NET_DEVICE_NAME);       // F8266-2
  _p->display.print(F(" SmartFarm"));
  for (uint8_t i = 0; i < OLED_INFO_ROWS; i++) {
    if (!_p->hasInfo[i]) continue;
    _p->display.setCursor(0, OLED_INFO_Y + i * 8);  // 每行 8 像素（size 1）
    _p->display.println(_p->infoLine[i]);
  }
  _p->display.display();
}
