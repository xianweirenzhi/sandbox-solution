#pragma once

// Gy39Sensor：气象二合一采集（GY-39：BME280 温/湿/气压 + MAX44009 光照），
// 替代原 SHT30（温湿度）+ GY-30（光照）两个 I2C 传感器，一个模块四项读数。
//
// 接法（直连芯片，绕过模块 MCU——现场实测采用的唯一接法）：
//   模块板边 SDA/SCL 焊盘 → 板上 SDA(GPIO4)/SCL(GPIO5)，与 OLED/SGP30 共线；
//   BME280@0x76/0x77（双地址探测，Adafruit 库含出厂校准补偿）读温/湿/气压，
//   MAX44009@0x4A 读光照（0x03/0x04 寄存器，lux = M·2^E·0.045 × 现场标定倍率）。
//   （UART / I2C-MCU(0x5B) 两条备选路径在直连方案联调通过后删除，历史见 README 更新记录。）
//
// 健康判定：未检测到不阻塞启动，每 10s 重扫（热插拔自愈）；连续失败标记故障，
//   恢复自动转好；读数超物理范围按失败处理。光照与温湿压独立跟踪有效性
//   （hasData()=温湿压、hasLux()=光照，任一缺失对应上报 null / OLED 显 --）。
// ★光照标定：本模块 MAX44009 实测读数恒偏低（兼容片灵敏度/模块窗口衰减的固定
//   倍率偏差），已按 F8266-1(BH1750) 并排对照标定为 ≈14 倍（环境光原始 ~5.4 ↔
//   参考 ~75），固件以 GY39_LUX_CAL(14) 修正；再精调与 F8266-1 同照度对照后改
//   .cpp 顶部该宏即可。
// 实现细节收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class Gy39Sensor {
 public:
  Gy39Sensor();
  ~Gy39Sensor();

  // 初始化（探测 BME280/MAX44009），未检测到不阻塞启动
  bool begin();

  // 周期维护：温湿压 + 光照采集、健康判定、热插拔重扫
  void handle();

  // 模块当前是否在线（任一芯片应答）且未处故障态
  bool isOk() const;

  // 温/湿/气压三项是否有效（true 时 getTempC/getHumRH/getPressPa 可用）
  bool hasData() const;

  // 光照是否有效（true 时 getLux 可用；与温湿压独立跟踪）
  bool hasLux() const;

  float getTempC() const;        // 最近一次有效温度（°C）
  float getHumRH() const;        // 最近一次有效湿度（%RH）
  unsigned long getPressPa() const;  // 最近一次有效大气压（Pa）
  float getLux() const;          // 最近一次有效光照（lux，已乘标定倍率）

 private:
  struct Impl;  // 实现体（定义在 .cpp）
  Impl *_p;
};
