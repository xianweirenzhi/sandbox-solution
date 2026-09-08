#pragma once

// Gy39Sensor：GY-39 气象二合一模块（板载 BME280 温度/湿度/气压 + MAX44009 光照），
// 替代原 SHT30（温湿度）+ GY-30（光照）两个 I2C 传感器，一个模块四项读数。
//
// 通信：★须为 I2C 模式（MCU_IIC：模块 S0 焊桥接 GND；出厂默认是 UART 模式，
//   用 I2C 前须改焊桥）。模块引脚 CT=SCL、DR=SDA，接到与 OLED/SGP30 共用的
//   I2C 总线（PERIPH_I2C_SDA/SCL），7 位地址 0x5B，内部 ~10Hz 自主刷新寄存器。
//
// 寄存器（官方手册，0x00 起连续 14 字节大端，换算同 UART 协议 /100）：
//   0x00~0x03 lux（32 位，0.01lux）| 0x04~0x05 温度（16 位有符号，0.01℃）
//   0x06~0x09 气压（32 位，0.01Pa）| 0x0A~0x0B 湿度（16 位，0.01%RH）
//   0x0C~0x0D 海拔（不使用）
//
// 健康判定：同 SHT30 模式——总线上未检测到不阻塞启动，每 10s 重扫（热插拔
//   自愈）；在总线但连续 3 次读取失败/读数超物理范围判故障，恢复自动转好。
// 实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Gy39Sensor {
 public:
  Gy39Sensor();
  ~Gy39Sensor();

  // 初始化（探测 0x5B；未检测到不阻塞启动，handle 内周期重扫）
  bool begin();

  // 周期维护：健康采集（2s）或未在总线时重扫（10s）
  void handle();

  // 链路健康：在总线且连续失败未达故障阈值
  bool isOk() const;

  // 是否已有可信读数（故障后清除，恢复重新置位）
  bool hasData() const;

  // 最近一次有效读数
  float getTempC() const;           // 温度 ℃
  float getHumRH() const;           // 湿度 %RH
  unsigned long getPressPa() const; // 大气压 Pa
  float getLux() const;             // 光照 lux

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
