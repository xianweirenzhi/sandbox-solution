#pragma once

// OledCtrl：智慧农场 OLED 状态可视化层。
// 职责：初始化 0.96" SSD1306（I2C），渲染标题区 + 多行信息区；各子系统把自身健康/读数
//       以一行文本写入 setInfoLine(row, ...)（如 "T:25.3C H:56%" / "SHT30 ERR"），
//       本模块统一上屏。只做显示，不掺业务采集逻辑。
// 说明：屏对象、I2C/驱动细节全部收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库
//       （String 仅前置声明，签名中只使用 const 引用）。

class String;  // 前置声明：仅用于接口签名（const String&），头文件不包含 Arduino.h

class OledCtrl {
 public:
  OledCtrl();
  ~OledCtrl();

  // 初始化 I2C 与屏幕，点亮并显示标题首屏；返回是否检测到屏
  bool begin();

  // 屏是否初始化成功（供主逻辑 / 串口查询）
  bool isOk() const;

  // 更新信息区某一行（row：0~3，标题区下方依次 4 行，对应子系统各占一行）：
  // 写入状态文本，行内容变化才重绘；传空串清除该行。row 越界忽略。
  void setInfoLine(uint8_t row, const String &text);

  // 周期维护（预留：后续整页子系统状态刷新；当前由 setInfoLine 按需驱动）
  void handle();

 private:
  void render();  // 按当前状态整屏重绘（标题区 + 信息行）

  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏 Adafruit_SSD1306 / Wire
  Impl *_p;
};
