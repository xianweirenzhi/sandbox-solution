#pragma once

// SoilSensor：土壤湿度监测（模拟量，AO 口 → 开发板唯一 ADC 引脚 A0）。
// 判定语义：AO 电压高 = 土壤阻值大 = 过干，AO 电压低 = 湿润适宜
//   （与 F8266-1 数字模块 DO 高=干 的极性一致，由 .cpp 顶部阈值宏控制，
//    换模块极性相反时对调两个阈值宏即可）：
//   isMoist() = true 即土壤湿润适宜，isDry() = true 即湿度过低（过干）。
// 防抖：滞回双阈值——滤波值低于 SOIL_MOIST_BELOW 才判湿、高于 SOIL_DRY_ABOVE
//       才判干，两阈值之间维持原判定，临界湿度附近的缓慢漂移不会引起状态翻转。
// 接线：模块 VCC→开发板 3V3，GND→GND，AO→[分压]→A0（DO 悬空）。
//       ★模块 AO 满摆幅约 3.3V，而 HUZZAH ADC 量程仅 0~1V——必须经分压
//         （现场采用 3×1kΩ 串联 + 1kΩ 的 4:1 链，3.3V→0.825V）再入 A0，
//         直连超量程会读数贴顶并可能损伤 ADC。
// 说明：模拟器件无“在线探测”概念（AO 拔线后 A0 悬空读数不可信，表现类似贴顶）。
//       实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class SoilSensor {
 public:
  SoilSensor();
  ~SoilSensor();

  // 初始化 ADC，并按首采滤波值给出初始判定（避免上电误报）
  bool begin();

  // 周期维护：中值滤波采集 + 滞回阈值判定
  void handle();

  // 当前生效判定：true = 土壤湿润适宜
  bool isMoist() const;

  // 当前生效判定：true = 土壤湿度过低（过干），恒等于 !isMoist()
  bool isDry() const;

  // 最近一次滤波后原始 ADC 值（0~1023，对应 A0 0~1.0V；标定/上屏用）
  int getRaw() const;

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
