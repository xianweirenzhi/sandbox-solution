#pragma once

// OledCtrl：智慧农场 OLED 状态可视化层（当前为业务层第一步：激活显示）。
// 职责：初始化 0.96" SSD1306（I2C），点亮并显示基础信息；后续各子系统把自身
//       健康/状态写入本模块，由本模块统一渲染（届时在 oled_ctrl.cpp 扩展）。
//       只做显示，不掺业务采集逻辑。
// 说明：屏对象、I2C/驱动细节全部收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class OledCtrl {
 public:
  OledCtrl();
  ~OledCtrl();

  // 初始化 I2C 与屏幕，点亮并显示激活首屏；返回是否检测到屏
  bool begin();

  // 屏是否初始化成功（供主逻辑 / 串口查询）
  bool isOk() const;

  // 周期维护（预留：后续子系统状态刷新上屏；当前无周期内容）
  void handle();

 private:
  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏 Adafruit_SSD1306 / Wire
  Impl *_p;
};
