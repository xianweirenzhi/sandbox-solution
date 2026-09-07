#pragma once

// Sht30Sensor：智慧农场第一个子系统——SHT30 温湿度采集（I2C，与 OLED 共线）。
// 职责：周期读取温度/湿度并判定传感器健康（连续读取失败即判故障，恢复自动转好），
//       为 OLED 状态屏 / 后续主机上报提供可信数据。只做采集，不掺显示与通信。
// 说明：传感器对象、I2C/驱动细节全部收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Sht30Sensor {
 public:
  Sht30Sensor();
  ~Sht30Sensor();

  // 初始化 I2C 并探测传感器（0x44）；返回是否在总线上检测到
  bool begin();

  // 周期维护：按采集周期读取；未检测到传感器时周期重扫（支持热插拔自愈）
  void handle();

  // 传感器是否健康：在总线上且近期读取连续失败未超阈值
  bool isOk() const;

  // 是否已有有效读数（上电首次读取完成前为 false）
  bool hasData() const;

  // 最近一次有效读数（温度 °C / 湿度 %RH；仅 hasData() 为 true 时有意义）
  float getTempC() const;
  float getHumRH() const;

 private:
  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏 Adafruit_SHT31 / Wire
  Impl *_p;
};
