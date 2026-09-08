#pragma once

// ActuatorCtrl：智慧农场执行实体模块——水泵 / 风扇 / 舵机（首个执行层子系统）。
// 接线（★执行器电源独立供电并共地）：
//   水泵继电器/MOS IN → GPIO0（高=开泵；GPIO0 烧录时拉低=泵关，安全；上电瞬间内部
//   上拉=高会短暂开启几十 ms，电机响应慢通常无感）
//   风扇双路驱动 IN1(正转)/IN2(反转) → GPIO13 / GPIO2（低=触发；GPIO2 为 boot 脚上电
//   需高，低触发设备上电默认关=安全）
//   舵机信号线（SG90 橙线）→ GPIO12（50Hz PWM）
// 安全设计：
//   ① 上电/初始化即进入安全态：泵关、风扇停、舵机回 0°（0°=大棚关，业务安全位）；
//   ② 风扇双路【软件互锁】——正反转任一开启时强制断开另一路，杜绝两路同时
//      触发导致驱动级电源直通短路（H 桥直通）；接线与后续命令均须遵守此约束。
//   ③ GPIO15 上电自带下拉（会让低触发模块上电误开），严禁用于低触发执行器。
// 说明：驱动对象、Servo 细节全部收敛在 .cpp（pimpl），头文件不依赖 Arduino / 库。

class ActuatorCtrl {
 public:
  ActuatorCtrl();
  ~ActuatorCtrl();

  // 初始化全部 GPIO 并进入安全态（泵关/风扇停/舵机 0°=关棚）；返回 true（无探测概念）
  bool begin();

  // 周期维护（预留：舵机缓动/看门狗自动停等；当前无周期任务）
  void handle();

  // ---- 水泵（高电平开启） ----
  void pumpOn();
  void pumpOff();
  bool pumpIsOn() const;

  // ---- 风扇（双路低触发正反转，软件互锁） ----
  void fanForward();   // 正转（自动断开反转）
  void fanReverse();   // 反转（自动断开正转）
  void fanStop();
  // 当前风扇状态：0=停 / 1=正转 / 2=反转
  uint8_t fanState() const;

  // ---- 舵机（180° 摆角） ----
  void servoSet(uint8_t deg);   // 0~180°（越界自动夹取）
  uint8_t servoGet() const;     // 当前设定角度

  // ---- 上电自检（阻塞式，setup 末尾调用一次） ----
  // 逐个激活执行器 0.5s：泵开→扇正转→扇反转→舵机摆 180°(开棚演示)→回关棚 0°，供人工检查。
  void selfTest();

  // ---- 全停安全态（★断连保护：与主机连接断开时调用一次） ----
  // 泵关 + 风扇停 + 舵机回 0°(大棚关)；重连后不自动恢复（需主机重新下发命令），
  // 防止失去控制通道时执行器保持危险状态（如水泵持续喷水）。
  void allStop();

 private:
  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏 Servo / Arduino
  Impl *_p;
};
