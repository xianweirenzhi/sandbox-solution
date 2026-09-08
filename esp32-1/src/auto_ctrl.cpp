#include "auto_ctrl.h"

#include <Preferences.h>
#include <math.h>

#include "config.h"
#include "port_service.h"

namespace auto_ctrl {

namespace {  // 内部实现细节

// 规则索引
enum RuleIdx {
  R_TEMP = 0,   // 高温→风扇
  R_CO2  = 1,   // 高CO₂→风扇
  R_SOIL = 2,   // 土壤干→水泵
  R_HUM  = 3,   // 低湿→水泵
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
  {"土壤浇水", 0, 0, false},
  {"低湿加湿", 0, 0, false},
};
bool        s_fanActive = false;       // 风扇聚合状态
bool        s_pumpActive = false;      // 水泵聚合状态
uint32_t    s_lastSampleMs = 0;        // 上次判断时刻
Preferences s_prefs;                   // NVS 存储

// 载入配置(NVS 无则用 config.h 默认)
void loadConfig() {
  if (!s_prefs.begin("autocfg", true)) {  // 只读打开失败则用默认
    s_cfg.enabled = true;
    s_cfg.tHigh   = AC_T_HIGH;
    s_cfg.co2High = AC_CO2_HIGH;
    s_cfg.hLow    = AC_H_LOW;
    return;
  }
  s_cfg.enabled = s_prefs.getBool("en", true);
  s_cfg.tHigh   = s_prefs.getFloat("tHi", AC_T_HIGH);
  s_cfg.co2High = s_prefs.getFloat("co2Hi", AC_CO2_HIGH);
  s_cfg.hLow    = s_prefs.getFloat("hLo", AC_H_LOW);
  s_prefs.end();
}

void saveConfig() {
  if (s_prefs.begin("autocfg", false)) {
    s_prefs.putBool("en", s_cfg.enabled);
    s_prefs.putFloat("tHi", s_cfg.tHigh);
    s_prefs.putFloat("co2Hi", s_cfg.co2High);
    s_prefs.putFloat("hLo", s_cfg.hLow);
    s_prefs.end();
  }
}

// 单条规则:返回当前是否越界;用 NaN 表示无数据
bool overTemp(float t)   { return !isnan(t) && t >= s_cfg.tHigh; }
bool safeTemp(float t)   { return !isnan(t) && t <= s_cfg.tHigh - AC_T_HYST; }
bool overCo2(float co2)  { return !isnan(co2) && co2 >= s_cfg.co2High; }
bool safeCo2(float co2)  { return !isnan(co2) && co2 <= s_cfg.co2High - AC_CO2_HYST; }
bool overSoil(int8_t s)  { return s == 0; }  // 0=干
bool safeSoil(int8_t s)  { return s == 1; }  // 1=湿
bool overHum(float h)    { return !isnan(h) && h < s_cfg.hLow; }
bool safeHum(float h)    { return !isnan(h) && h >= s_cfg.hLow + AC_H_HYST; }

// 用连续计数更新单条规则:over=当前越界, safe=当前回安全区, 其余(无数据)保持
void updateRule(Rule &r, bool over, bool safe) {
  if (over) { r.hiCnt++; r.loCnt = 0; }
  else if (safe) { r.loCnt++; r.hiCnt = 0; }
  else { return; }                      // 无数据:保持现状
  if (!r.active && r.hiCnt >= AC_CONFIRM_N) r.active = true;
  else if (r.active && r.loCnt >= AC_CONFIRM_N) r.active = false;
}

void sendCmd(const char* cmd) {
  port_service::send(0, String(cmd));   // idx 0 = 8000 = F8266-1
}

}  // namespace

void begin() {
  loadConfig();
  s_fanActive = false;
  s_pumpActive = false;
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
  updateRule(s_rules[R_SOIL], overSoil(s.soil), safeSoil(s.soil));
  updateRule(s_rules[R_HUM],  overHum(s.h),    safeHum(s.h));

  // 聚合 + 状态翻转才下发
  bool fan  = s_rules[R_TEMP].active || s_rules[R_CO2].active;
  bool pump = s_rules[R_SOIL].active || s_rules[R_HUM].active;

  if (fan != s_fanActive) {
    s_fanActive = fan;
    sendCmd(fan ? "FAN FWD" : "FAN STOP");
  }
  if (pump != s_pumpActive) {
    s_pumpActive = pump;
    sendCmd(pump ? "PUMP ON" : "PUMP OFF");
  }
}

Config getConfig() { return s_cfg; }

void setConfig(const Config &c) {
  s_cfg = c;
  saveConfig();
}

bool fanActive()  { return s_fanActive; }
bool pumpActive() { return s_pumpActive; }

uint8_t ruleCount() { return R_COUNT; }

RuleState ruleState(uint8_t idx) {
  RuleState st;
  if (idx >= R_COUNT) return st;
  st.name   = s_rules[idx].name;
  st.active = s_rules[idx].active;
  return st;
}

}  // namespace auto_ctrl
