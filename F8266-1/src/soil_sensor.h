#pragma once

// SoilSensor：智慧农场子系统④——土壤湿度监测（数字量，DO 口）。
// 判定语义（★按现场模块实测设定：低电平=湿润适宜、高电平=过干；
//   由 .cpp 顶部 SOIL_HIGH_IS_MOIST 宏控制，换模块极性相反时改宏即可）：
//   isMoist() = true 即土壤湿润适宜，isDry() = true 即湿度过低（过干）。
// 消抖：环境消抖 1s——新电平须连续稳定保持 SOIL_DEBOUNCE_MS 才切换生效状态，
//       临界湿度附近的高频跳变（浇水边缘/电极抖动）不会引起状态翻转。
// 接线：模块 VCC→开发板 3V3（保证 DO 高电平 3.3V），GND→GND，DO→GPIO14；AO 悬空。
//       ★避开 GPIO0/2/15（ESP8266 boot 启动约束脚）。
// 说明：数字量两态均合法，无“在线探测”概念（拔线表现为状态冻结而非报错）。
//       实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class SoilSensor {
 public:
  SoilSensor();
  ~SoilSensor();

  // 初始化 GPIO 输入，并以当前电平作为初始稳定状态（避免上电误报）
  bool begin();

  // 周期维护：读取 DO 并做 1s 环境消抖
  void handle();

  // 当前生效（已消抖）判定：true = 土壤湿润适宜
  bool isMoist() const;

  // 当前生效判定：true = 土壤湿度过低（过干），恒等于 !isMoist()
  bool isDry() const;

  // 最近一次原始电平（未消抖，HIGH/LOW，调试用）
  int rawLevel() const;

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
