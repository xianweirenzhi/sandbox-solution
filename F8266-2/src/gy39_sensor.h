#pragma once

// Gy39Sensor：气象二合一采集（GY-39：BME280 温/湿/气压 + MAX44009 光照），
// 替代原 SHT30（温湿度）+ GY-30（光照）两个 I2C 传感器，一个模块四项读数。
//
// 三种接法固件自适应（VCC→3V3、GND→GND 不变），探测先到先用、接线热切换自愈：
//   ① 直连芯片（★实测推荐，S1 焊桥 GND / 模块板边自带 I2C 焊盘）：模块 Pin7(SDA)→板上 SDA、
//      Pin8(SCL)→板上 SCL，BME280（0x76/0x77，温/湿/气压，Adafruit 库含补偿计算）
//      + MAX44009（0x4A，光照，直读 0x03/0x04 寄存器 lux=M·2^E·0.045）绕过模块 MCU
//   ② UART 模式（出厂默认，免焊）：模块 CT(=TX)→PERIPH_GY39_UART_RX(GPIO14)，
//      DR 不接；9600bps 主动持续发帧：5A 5A <type> <len> <data[len]> <sum>
//      （0x15=光照 4B、0x45=温度2B+气压4B+湿度2B+海拔2B，大端 /100，累加和校验）
//   ③ I2C-MCU 模式（S0 焊桥 GND）：CT→SCL、DR→SDA，7 位地址 0x5B，
//      直读 0x00~0x0D 数据寄存器（14B 大端：lux/T/P/HUM）
//
// 健康判定：同 SHT30 模式——总线上未检测到/无有效帧不阻塞启动，周期重扫（热插拔
//   自愈）；连续失败标记故障，恢复自动转好。读数超物理范围按失败处理（防总线毛刺
//   污染缓存值）。
// 说明：温/湿/气压三项同帧（0x45 / I2C 整块），光照独立帧（0x15 / 同块独立字段），
//   两路有效性分别跟踪（hasData()=温湿压、hasLux()=光照，任一缺失对应上报 null）。
//   实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Gy39Sensor {
 public:
  Gy39Sensor();
  ~Gy39Sensor();

  // 初始化（I2C 探测 + 启动 UART 监听），未检测到不阻塞启动
  bool begin();

  // 周期维护：UART 字节流解析 + I2C 探测/采集 + 健康判定
  void handle();

  // 模块当前是否在线（I2C 在总线 / UART 帧流存活）且未处故障态
  bool isOk() const;

  // 温/湿/气压三项是否有效（true 时 getTempC/getHumRH/getPressPa 可用）
  bool hasData() const;

  // 光照是否有效（true 时 getLux 可用；与温湿压独立跟踪）
  bool hasLux() const;

  float getTempC() const;        // 最近一次有效温度（°C）
  float getHumRH() const;        // 最近一次有效湿度（%RH）
  unsigned long getPressPa() const;  // 最近一次有效大气压（Pa）
  float getLux() const;          // 最近一次有效光照（lux）

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
