#pragma once

// Mq2Sensor：智慧农场子系统③——MQ-2 可燃气体/烟雾采集（模拟量，ADC 读取）。
// 职责：周期读取 A0 原始值（中值滤波），管理预热状态、浓度阈值报警与读数合理性监测，
//       为 OLED 状态屏 / 后续主机上报提供数据。只做采集，不掺显示与通信。
// 接线（★独立 5V 电源模块供电，开发板只读 ADC）：
//   MQ-2 VCC/GND → 独立 5V 电源；GND 必须与开发板 GND 共地（否则 ADC 无参考，读数无效）。
//   MQ-2 AO ──[5×1kΩ 串联]──┬──[1kΩ]── GND（5:1 分压），┬ 处接 HUZZAH ADC（A0，
//   量程 0~1V，分压后 5V→0.83V）。MQ-2 DO 悬空（模拟量方案不用）。
// 说明：ADC 无“在总线”探测（模拟器件），健康模型为读数合理性监测，详见 .cpp。
//       实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Mq2Sensor {
 public:
  Mq2Sensor();
  ~Mq2Sensor();

  // 初始化状态机（预热计时从此开始；模拟器件无可探测的“在线”概念）
  bool begin();

  // 周期维护：按采集周期中值滤波采样，更新预热/报警/合理性判定
  void handle();

  // 是否预热中（上电后 MQ2_PREHEAT_MS 内；此期间读数无意义，不参与报警判定）
  bool isPreheat() const;

  // 读数是否合理（未出现“长期贴 0/贴满量程”的断线/接错迹象；预热期恒为 true）
  bool isOk() const;

  // 是否浓度超阈值（预热结束后，滤波值 > MQ2_ALARM_RAW）
  bool isAlarm() const;

  // 最近一次滤波后的原始 ADC 值（0~1023，对应 A0 0~1V）
  int getRaw() const;

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
