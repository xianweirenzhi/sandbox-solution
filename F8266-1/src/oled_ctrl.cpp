#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "net_config.h"   // NET_DEVICE_NAME
#include "oled_ctrl.h"

// ===================== 显示参数（改接线/地址只动这里）=====================
// HUZZAH 板上 I2C 排针：SDA=GPIO4、SCL=GPIO5
#define OLED_I2C_SDA   4
#define OLED_I2C_SCL   5
// 0.96" SSD1306 常见 I2C 地址 0x3C（部分模组为 0x3D）
#define OLED_I2C_ADDR  0x3C
#define OLED_WIDTH     128
#define OLED_HEIGHT    64

// ===================== 实现体 =====================
struct OledCtrl::Impl {
  Adafruit_SSD1306 display = Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire);
  bool ok = false;   // 屏是否初始化成功
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

bool OledCtrl::begin() {
  Wire.begin(OLED_I2C_SDA, OLED_I2C_SCL);

  if (!_p->display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    _p->ok = false;
    Serial.println(F("[oled] 未检测到 SSD1306（检查 I2C 接线 / 地址 0x3C/0x3D）"));
    return false;
  }
  _p->ok = true;

  // 激活首屏：设备名 / 项目名 / OLED 状态。
  // 后续“子系统状态展示”在此扩展：采集各模块健康写入屏幕区域后 display() 刷新。
  // 文字统一 ASCII/英文（Adafruit GFX 无内置中文字库，128×64 小屏以简短英文为宜）。
  _p->display.clearDisplay();
  _p->display.setTextSize(1);
  _p->display.setTextColor(SSD1306_WHITE);
  _p->display.setCursor(0, 0);
  _p->display.println(NET_DEVICE_NAME);     // F8266-1
  _p->display.println(F("SmartFarm"));
  _p->display.println(F("OLED OK"));
  _p->display.display();

  Serial.println(F("[oled] SSD1306 已激活"));
  return true;
}

void OledCtrl::handle() {
  // 预留：后续各子系统把健康/状态写入本模块后，在本函数按周期重绘上屏。
  // 激活阶段无周期刷新内容，保持空实现，仅维持 main loop 统一调用习惯。
}
