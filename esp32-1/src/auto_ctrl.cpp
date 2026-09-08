#include "auto_ctrl.h"

#include <Preferences.h>
#include <math.h>

#include "config.h"
#include "port_service.h"

namespace auto_ctrl {

namespace {  // 内部实现细节

// 规则索引
enum RuleIdx {
  R_TEMP = 0,   // 高温→风扇正转
  R_CO2  = 1,   // 高CO₂→风扇正转
  R_HUMH = 2,   // 高湿→风扇反转排湿
  R_SOIL = 3,   // 土壤干→水泵
  R_COUNT = 4,
};

struct Rule {
  const char* name;
  int   hiCnt;        // 连续越界次数
  int   loCnt;        // 连续回安全区次数
  bool  active;
};

Config      s_cfg;                     // 运行配置
Rule        s_rules[R_COUNT] = {
  {"高温通风", 0, 0, false},
  {"高CO₂通风", 0, 0, false},
  {"高湿排湿", 0, 0, false},
  {"土壤浇水", 0, 0, false},
};
// 大棚(舵机)独立防抖计数
int         s_ghHiCnt = 0, s_ghLoCnt = 0;
bool        s_ghActive = false;        // 大棚应开(舵机 180°)
// 期望状态(用于校正下发)
bool        s_pumpActive = false;      // 水泵应开
uint8_t     s_fanDir = 0;              // 风扇应 0=停 1=正转 2=反转
uint32_t    s_lastSampleMs = 0;        // 上次判断时刻
Preferences s_prefs;                   // NVS 存储

// 大棚超阈倍率 = 1 + AC_OVER_PCT/100 (默认 1.2)
const float AC_OVER_FACTOR = 1.0f + AC_OVER_PCT / 100.0f;

// 载入配置(NVS 无则用 config.h 默认)
void loadConfig() {
  if (!s_prefs.begin("autocfg", true)) {  // 只读打开失败则用默认
    s_cfg.enabled = true;
    s_cfg.tHigh   = AC_T_HIGH;
    s_cfg.co2High = AC_CO2_HIGH;
    s_cfg.hHigh   = AC_H_HIGH;
    return;
  }
  s_cfg.enabled = s_prefs.getBool("en", true);
  s_cfg.tHigh   = s_prefs.getFloat("tHi", AC_T_HIGH);
  s_cfg.co2High = s_prefs.getFloat("co2Hi", AC_CO2_HIGH);
  s_cfg.hHigh   = s_prefs.getFloat("hHi", AC_H_HIGH);   // 新键(旧 hLo 已弃用)
  s_prefs.end();
}

void saveConfig() {
  if (s_prefs.begin("autocfg", false)) {
    s_prefs.putBool("en", s_cfg.enabled);
    s_prefs.putFloat("tHi", s_cfg.tHigh);
    s_prefs.putFloat("co2Hi", s_cfg.co2High);
    s_prefs.putFloat("hHi", s_cfg.hHigh);
    s_prefs.end();
  }
}

// 单条规则:返回当前是否越界;用 NaN 表示无数据
bool overTemp(float t)   { return !isnan(t) && t >= s_cfg.tHigh; }
bool safeTemp(float t)   { return !isnan(t) && t <= s_cfg.tHigh - AC_T_HYST; }
bool overCo2(float co2)  { return !isnan(co2) && co2 >= s_cfg.co2High; }
bool safeCo2(float co2)  { return !isnan(co2) && co2 <= s_cfg.co2High - AC_CO2_HYST; }
bool overHum(float h)    { return !isnan(h) && h >= s_cfg.hHigh; }
bool safeHum(float h)    { return !isnan(h) && h <= s_cfg.hHigh - AC_H_HYST; }
bool overSoil(int8_t s)  { return s == 0; }  // 0=干
bool safeSoil(int8_t s)  { return s == 1; }  // 1=湿

// 大棚(舵机)越界/回安全区(滞回用 AC_OVER_FACTOR)
bool overGh(float t, float co2, float h) {
  return (overTemp(t) && t >= s_cfg.tHigh * AC_OVER_FACTOR) ||
         (overCo2(co2) && co2 >= s_cfg.co2High * AC_OVER_FACTOR) ||
         (overHum(h) && h >= s_cfg.hHigh * AC_OVER_FACTOR);
}
// 大棚关闭需三者都回落到各自阈值以内(带一点余量防抖:≤阈值)
bool safeGh(float t, float co2, float h) {
  bool tIn  = isnan(t)  || t  <= s_cfg.tHigh;
  bool cIn  = isnan(co2)|| co2 <= s_cfg.co2High;
  bool hIn  = isnan(h)  || h  <= s_cfg.hHigh;
  return tIn && cIn && hIn;   // 全回阈内才算安全;中间地带保持现状
}

// 用连续计数更新单条规则:over=当前越界, safe=当前回安全区, 其余(无数据)保持
void updateRule(Rule &r, bool over, bool safe) {
  if (over) { r.hiCnt++; r.loCnt = 0; }
  else if (safe) { r.loCnt++; r.hiCnt = 0; }
  else { return; }                      // 无数据:保持现状
  if (!r.active && r.hiCnt >= AC_CONFIRM_N) r.active = true;
  else if (r.active && r.loCnt >= AC_CONFIRM_N) r.active = false;
}

// 大棚防抖(独立计数)
void updateGh(bool over, bool safe) {
  if (over) { s_ghHiCnt++; s_ghLoCnt = 0; }
  else if (safe) { s_ghLoCnt++; s_ghHiCnt = 0; }
  else return;
  if (!s_ghActive && s_ghHiCnt >= AC_CONFIRM_N) s_ghActive = true;
  else if (s_ghActive && s_ghLoCnt >= AC_CONFIRM_N) s_ghActive = false;
}

void sendCmd(const char* cmd) {
  port_service::send(0, String(cmd));   // idx 0 = 8000 = F8266-1
}

}  // namespace

void begin() {
  loadConfig();
  s_pumpActive = false;
  s_fanDir = 0;
  s_ghActive = false;
  s_ghHiCnt = s_ghLoCnt = 0;
  s_lastSampleMs = 0;
}

void loop() {
  uint32_t now = millis();
  if (now - s_lastSampleMs < AC_SAMPLE_MS) return;
  s_lastSampleMs = now;

  if (!s_cfg.enabled) {                 // 手动模式:规则不动作(保持现状)
    return;
  }

  PortSnapshot s = port_service::snapshot(0);
  if (!s.hasData) return;               // 尚无传感数据

  updateRule(s_rules[R_TEMP], overTemp(s.t), safeTemp(s.t));
  updateRule(s_rules[R_CO2],  overCo2(s.co2),  safeCo2(s.co2));
  updateRule(s_rules[R_HUMH], overHum(s.h),    safeHum(s.h));
  updateRule(s_rules[R_SOIL], overSoil(s.soil), safeSoil(s.soil));
  updateGh(overGh(s.t, s.co2, s.h), safeGh(s.t, s.co2, s.h));

  // ---- 期望状态(由规则聚合) ----
  bool heat = s_rules[R_TEMP].active || s_rules[R_CO2].active;  // 散热
  bool wet  = s_rules[R_HUMH].active;                            // 排湿
  s_pumpActive = s_rules[R_SOIL].active;

  // 风扇方向:高温/高CO₂ 散热优先 → 正转;否则高湿排湿 → 反转;否则停
  if (heat)        s_fanDir = 1;
  else if (wet)    s_fanDir = 2;
  else             s_fanDir = 0;

  // ---- 每周期「期望 vs 从机实际回报」校正(不只翻转时下发) ----
  // 覆盖手动篡改/丢包/自动↔手动切换导致的失配;失配持续则每周期补发(自愈)。
  if (s.online && s.hasData) {
    // 风扇:期望方向 0/1/2 与实际 fan 回报 0/1/2 比对
    if (s.fan != s_fanDir) {
      if (s_fanDir == 1) sendCmd("FAN FWD");
      else if (s_fanDir == 2) sendCmd("FAN REV");
      else sendCmd("FAN STOP");
    }
    // 水泵
    uint8_t wantPump = s_pumpActive ? 1 : 0;
    if (s.pump != wantPump) sendCmd(s_pumpActive ? "PUMP ON" : "PUMP OFF");
    // 大棚(舵机):期望 180(开)/0(关)
    uint8_t wantServo = s_ghActive ? 180 : 0;
    if (s.servo != wantServo) sendCmd(s_ghActive ? "SERVO 180" : "SERVO 0");
  }
}

Config getConfig() { return s_cfg; }

void setConfig(const Config &c) {
  s_cfg = c;
  saveConfig();
}

bool fanActive()  { return s_fanDir != 0; }
uint8_t fanDir()  { return s_fanDir; }
bool pumpActive() { return s_pumpActive; }
bool servoActive(){ return s_ghActive; }

uint8_t ruleCount() { return R_COUNT; }

RuleState ruleState(uint8_t idx) {
  RuleState st;
  if (idx >= R_COUNT) return st;
  st.name   = s_rules[idx].name;
  st.active = s_rules[idx].active;
  return st;
}

}  // namespace auto_ctrl
