#include <Arduino.h>
#include <Servo.h>

#include "actuator_ctrl.h"

// ===================== 引脚与极性参数（改接线只动这里）=====================
#define PUMP_PIN      0     // 水泵驱动 IN（低电平开启）；★GPIO0 为烧录脚，烧录/上传固件时被拉低→水泵会误开喷水，烧录前务必断水断电（用户已知情接受）
#define FAN_FWD_PIN   13    // 风扇正转驱动 IN（低电平触发）
#define FAN_REV_PIN   16    // 风扇反转驱动 IN（低电平触发）
#define SERVO_PIN     12    // 舵机信号线（50Hz PWM）

// 低触发语义：LOW=开启/触发，HIGH=关闭/停止（上拉默认 HIGH 即安全态）
#define TRIG_ON   LOW
#define TRIG_OFF  HIGH

#define SERVO_CENTER_DEG  90    // 上电/初始化舵机中位角

// ===================== 实现体 =====================
struct ActuatorCtrl::Impl {
  Servo    servo;              // 舵机（attach 后由库生成 50Hz PWM）
  bool     servoAttached = false;
  uint8_t  servoDeg = SERVO_CENTER_DEG;
  bool     pumpOn = false;
  uint8_t  fan = 0;            // 0=停 / 1=正转 / 2=反转
};

// ===================== 内部小工具 =====================

// 拉高一路驱动使其关闭（安全态写法：先写 HIGH 再改 OUTPUT，上电即无毛刺）
static void pinInitSafe(uint8_t pin) {
  digitalWrite(pin, TRIG_OFF);   // 先置关闭电平
  pinMode(pin, OUTPUT);          // 再切输出（切换瞬间即为关，不会误触发）
  digitalWrite(pin, TRIG_OFF);
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
  pinInitSafe(PUMP_PIN);
  pinInitSafe(FAN_FWD_PIN);
  pinInitSafe(FAN_REV_PIN);
  _p->pumpOn = false;
  _p->fan = 0;

  // 2) 舵机回中位（attach 后库立即输出 50Hz 脉冲）
  _p->servo.attach(SERVO_PIN);
  _p->servoAttached = true;
  _p->servoDeg = SERVO_CENTER_DEG;
  _p->servo.write(_p->servoDeg);

  Serial.println(F("[act] 执行器就绪（安全态：泵关/风扇停/舵机90°）"));
  return true;
}

void ActuatorCtrl::handle() {
  // 预留：舵机缓动、水泵超时自动停等安全逻辑；当前无周期任务。
}

// ---- 水泵 ----

void ActuatorCtrl::pumpOn() {
  digitalWrite(PUMP_PIN, TRIG_ON);
  _p->pumpOn = true;
  Serial.println(F("[act] 水泵 开"));
}

void ActuatorCtrl::pumpOff() {
  digitalWrite(PUMP_PIN, TRIG_OFF);
  _p->pumpOn = false;
  Serial.println(F("[act] 水泵 关"));
}

// ---- 风扇（正反转软件互锁：开任一路前先断另一路，防驱动级直通短路） ----

void ActuatorCtrl::fanForward() {
  digitalWrite(FAN_REV_PIN, TRIG_OFF);   // 互锁：先确保反转断开
  digitalWrite(FAN_FWD_PIN, TRIG_ON);
  if (_p->fan != 1) Serial.println(F("[act] 风扇 正转"));
  _p->fan = 1;
}

void ActuatorCtrl::fanReverse() {
  digitalWrite(FAN_FWD_PIN, TRIG_OFF);   // 互锁：先确保正转断开
  digitalWrite(FAN_REV_PIN, TRIG_ON);
  if (_p->fan != 2) Serial.println(F("[act] 风扇 反转"));
  _p->fan = 2;
}

void ActuatorCtrl::fanStop() {
  digitalWrite(FAN_FWD_PIN, TRIG_OFF);
  digitalWrite(FAN_REV_PIN, TRIG_OFF);
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
