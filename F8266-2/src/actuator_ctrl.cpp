#include <Arduino.h>
#include <Servo.h>

#include "actuator_ctrl.h"

// ===================== 引脚与极性参数（改接线只动这里）=====================
#define PUMP_PIN      0     // 水泵驱动 IN（高电平开泵，关=引脚浮空 INPUT，靠驱动模块输入下拉保持关）
#define FAN_FWD_PIN   13    // 风扇正转驱动 IN（低电平触发）
#define FAN_REV_PIN   2     // 风扇反转驱动 IN（低电平触发；GPIO2 为 boot 脚上电需高，低触发设备上电默认关=安全）
#define SERVO_PIN     12    // 舵机信号线（50Hz PWM）

// 触发极性（水泵高触发·浮空关断、风扇低触发，各自独立）：
#define PUMP_ON    HIGH   // 水泵：输出高电平开泵；关泵=切回浮空输入（非驱动低电平）
#define FAN_ON     LOW    // 风扇：低电平触发（开）
#define FAN_OFF    HIGH   // 风扇：高电平停止（安全态）

#define SERVO_CENTER_DEG  0     // 舵机安全位=大棚关(业务语义:0°=关棚 180°=开棚;原 90° 中位已弃)

// ===================== 实现体 =====================
struct ActuatorCtrl::Impl {
  Servo    servo;              // 舵机（attach 后由库生成 50Hz PWM）
  bool     servoAttached = false;
  uint8_t  servoDeg = SERVO_CENTER_DEG;
  bool     pumpOn = false;
  uint8_t  fan = 0;            // 0=停 / 1=正转 / 2=反转
};

// ===================== 内部小工具 =====================

// 置一路驱动为关闭电平（安全态写法：先写关闭电平再切 OUTPUT，上电/初始化无毛刺）。
// offLevel 取该路驱动的关闭电平：水泵=PUMP_OFF(LOW)、风扇=FAN_OFF(HIGH)。
static void pinInitSafe(uint8_t pin, int offLevel) {
  digitalWrite(pin, offLevel);   // 先置关闭电平
  pinMode(pin, OUTPUT);          // 再切输出（切换瞬间即为关，不会误触发）
  digitalWrite(pin, offLevel);
}

// ===================== 公开接口 =====================

ActuatorCtrl::ActuatorCtrl() : _p(new Impl) {}

ActuatorCtrl::~ActuatorCtrl() {
  delete _p;
  _p = nullptr;
}

bool ActuatorCtrl::pumpIsOn() const {
  return _p->pumpOn;
}

uint8_t ActuatorCtrl::fanState() const {
  return _p->fan;
}

uint8_t ActuatorCtrl::servoGet() const {
  return _p->servoDeg;
}

bool ActuatorCtrl::begin() {
  // 1) 水泵/风扇全部进入安全态（关闭）
  pinMode(PUMP_PIN, INPUT);           // 水泵关=浮空（模块输入下拉保持关）
  pinInitSafe(FAN_FWD_PIN, FAN_OFF);
  pinInitSafe(FAN_REV_PIN, FAN_OFF);
  _p->pumpOn = false;
  _p->fan = 0;

  // 2) 舵机回安全位（attach 后库立即输出 50Hz 脉冲；0°=大棚关）
  _p->servo.attach(SERVO_PIN);
  _p->servoAttached = true;
  _p->servoDeg = SERVO_CENTER_DEG;
  _p->servo.write(_p->servoDeg);

  Serial.println(F("[act] 执行器就绪（安全态：泵关/风扇停/舵机0°关棚）"));
  return true;
}

void ActuatorCtrl::handle() {
  // 预留：舵机缓动、水泵超时自动停等安全逻辑；当前无周期任务。
}

// ---- 水泵（高触发·浮空关断：开=输出高，关=引脚切回浮空输入） ----
// ★关断不主动驱动低电平：关态引脚 Hi-Z，由驱动模块输入端下拉保持关；
//   上电/复位/烧录期间引脚均为输入态，泵默认关（前提：模块输入下拉存在）。

void ActuatorCtrl::pumpOn() {
  digitalWrite(PUMP_PIN, PUMP_ON);    // 先写高（此时仍输入态，引脚不变）
  pinMode(PUMP_PIN, OUTPUT);          // 再切输出 → 无毛刺输出高电平开泵
  _p->pumpOn = true;
  Serial.println(F("[act] 水泵 开"));
}

void ActuatorCtrl::pumpOff() {
  pinMode(PUMP_PIN, INPUT);           // 切回浮空输入（Hi-Z）= 关泵
  _p->pumpOn = false;
  Serial.println(F("[act] 水泵 关"));
}

// ---- 风扇（低触发：低=开；正反转软件互锁：开任一路前先断另一路，防驱动级直通短路） ----

void ActuatorCtrl::fanForward() {
  digitalWrite(FAN_REV_PIN, FAN_OFF);   // 互锁：先确保反转断开
  digitalWrite(FAN_FWD_PIN, FAN_ON);
  if (_p->fan != 1) Serial.println(F("[act] 风扇 正转"));
  _p->fan = 1;
}

void ActuatorCtrl::fanReverse() {
  digitalWrite(FAN_FWD_PIN, FAN_OFF);   // 互锁：先确保正转断开
  digitalWrite(FAN_REV_PIN, FAN_ON);
  if (_p->fan != 2) Serial.println(F("[act] 风扇 反转"));
  _p->fan = 2;
}

void ActuatorCtrl::fanStop() {
  digitalWrite(FAN_FWD_PIN, FAN_OFF);
  digitalWrite(FAN_REV_PIN, FAN_OFF);
  if (_p->fan != 0) Serial.println(F("[act] 风扇 停"));
  _p->fan = 0;
}

// ---- 舵机 ----

void ActuatorCtrl::servoSet(uint8_t deg) {
  if (deg > 180) deg = 180;              // 夹取到 0~180°
  _p->servoDeg = deg;
  if (!_p->servoAttached) {
    _p->servo.attach(SERVO_PIN);
    _p->servoAttached = true;
  }
  _p->servo.write(deg);
  Serial.print(F("[act] 舵机 "));
  Serial.print(deg);
  Serial.println(F("°"));
}

// ---- 上电自检 ----

void ActuatorCtrl::selfTest() {
  Serial.println(F("[act] ==== 上电自检：逐个激活执行器 0.5s ===="));

  pumpOn();                       // 水泵开 0.5s（高触发）
  delay(500);
  pumpOff();
  delay(200);

  fanForward();                   // 风扇正转 0.5s（低触发）
  delay(500);
  fanStop();
  delay(200);

  fanReverse();                   // 风扇反转 0.5s
  delay(500);
  fanStop();
  delay(200);

  servoSet(180);                  // 舵机摆到 180°(开棚演示) 0.5s 后回关棚 0°
  delay(500);
  servoSet(SERVO_CENTER_DEG);

  Serial.println(F("[act] ==== 自检完成，回到安全态（泵关/风扇停/舵机0°关棚）===="));
}

// ---- 全停安全态（断连保护） ----

void ActuatorCtrl::allStop() {
  pumpOff();
  fanStop();
  servoSet(SERVO_CENTER_DEG);   // 舵机回关棚 0°
  Serial.println(F("[act] 主机连接断开 → 全停安全态（泵关/风扇停/舵机0°关棚）"));
}
