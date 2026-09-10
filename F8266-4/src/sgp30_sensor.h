#pragma once

// Sgp30Sensor：智慧农场子系统⑤——SGP30 空气质量采集（eCO₂ 等效值 + TVOC，I2C 0x58 共线）。
// ★定性说明：SGP30 实测 TVOC，eCO₂ 为其推算的【等效值】（非真实 CO₂ 浓度），
//   酒精/香水/可燃气体等 VOC 会使读数虚高——作空气新鲜度参考；后续从机如需真
//   CO₂（如温室施肥监控）应选 SCD40 等光声/NDIR 传感器。
// 职责：周期读取 eCO₂(ppm)/TVOC(ppb) 并判定传感器健康（连续失败判故障，恢复转好），
//       为 OLED 状态屏 / 后续主机上报提供数据。只做采集，不掺显示与通信。
// 说明：传感器对象、I2C/驱动细节全部收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Sgp30Sensor {
 public:
  Sgp30Sensor();
  ~Sgp30Sensor();

  // 初始化 I2C 并探测传感器（0x58）；返回是否在总线上检测到
  bool begin();

  // 周期维护：按采集周期读取；未检测到传感器时周期重扫（支持热插拔自愈）
  void handle();

  // 传感器是否健康：在总线上且近期读取连续失败未超阈值
  bool isOk() const;

  // 是否暖机中（上电 15s 内读数不可靠；此期间不判故障、读数仅供参考）
  bool isWarmup() const;

  // 是否已有有效读数（暖机完成且至少一次读取成功）
  bool hasData() const;

  // 最近一次有效读数：等效 CO₂（ppm）/ 总挥发性有机物（ppb）
  uint32_t getEco2Ppm() const;
  uint32_t getTvocPpb() const;

 private:
  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏 Adafruit_SGP30 / Wire
  Impl *_p;
};
