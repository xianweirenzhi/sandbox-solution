#include "auto_ctrl.h"

#include <Preferences.h>
#include <math.h>

#include "config.h"
#include "port_service.h"

namespace auto_ctrl {

namespace {  // 内部实现细节

// 规则索引(六个端口共用同一张规则名表)
enum RuleIdx {
  R_TEMP = 0,   // 高温→风扇正转
  R_CO2  = 1,   // 高CO₂→风扇正转
  R_HUMH = 2,   // 高湿→风扇反转排湿
  R_SOIL = 3,   // 土壤干→水泵
  R_COUNT = 4,
};
const char* const RULE_NAMES[R_COUNT] = {
  "高温通风", "高CO₂通风", "高湿排湿", "土壤浇水",
};

struct Rule {
  int   hiCnt = 0;    // 连续越界次数
  int   loCnt = 0;    // 连续回安全区次数
  bool  active = false;
};

// 每个从机端口一套独立实例(配置/规则/防抖计数/期望状态/采样时刻)
struct PortAuto {
  Config   cfg;
  Rule     rules[R_COUNT];
  int      ghHiCnt = 0, ghLoCnt = 0;   // 大棚(舵机)独立防抖计数
  bool     ghActive = false;           // 大棚应开(舵机 180°)
  bool     pumpActive = false;         // 水泵应开
  uint8_t  fanDir = 0;                 // 风扇应 0=停 1=正转 2=反转
  uint32_t lastSampleMs = 0;           // 上次判断时刻
};

PortAuto    s_pa[PORT_COUNT];
Preferences s_prefs;

// 大棚超阈倍率 = 1 + AC_OVER_PCT/100 (默认 1.2)
const float AC_OVER_FACTOR = 1.0f + AC_OVER_PCT / 100.0f;

// NVS 键名带端口序号后缀(en0/tHi0/co2Hi0/hHi0/...),同一 "autocfg" 命名空间
void loadConfig(uint8_t idx) {
  Config &c = s_pa[idx].cfg;
  char k[10];
  snprintf(k, sizeof(k), "en%u", idx);     c.enabled = s_prefs.getBool(k, true);
  snprintf(k, sizeof(k), "tHi%u", idx);    c.tHigh   = s_prefs.getFloat(k, AC_T_HIGH);
  snprintf(k, sizeof(k), "co2Hi%u", idx);  c.co2High = s_prefs.getFloat(k, AC_CO2_HIGH);
  snprintf(k, sizeof(k), "hHi%u", idx);    c.hHigh   = s_prefs.getFloat(k, AC_H_HIGH);
  snprintf(k, sizeof(k), "sWt%u", idx);    c.soilWet = s_prefs.getFloat(k, AC_SOIL_WET);
  snprintf(k, sizeof(k), "sDr%u", idx);    c.soilDry = s_prefs.getFloat(k, AC_SOIL_DRY);
}

// 单条规则:返回当前是否越界;用 NaN 表示无数据(阈值取自对应端口配置)
bool overTemp(const Config &c, float t)  { return !isnan(t) && t >= c.tHigh; }
bool safeTemp(const Config &c, float t)  { return !isnan(t) && t <= c.tHigh - AC_T_HYST; }
bool overCo2(const Config &c, float v)   { return !isnan(v) && v >= c.co2High; }
bool safeCo2(const Config &c, float v)   { return !isnan(v) && v <= c.co2High - AC_CO2_HYST; }
bool overHum(const Config &c, float h)   { return !isnan(h) && h >= c.hHigh; }
bool safeHum(const Config &c, float h)   { return !isnan(h) && h <= c.hHigh - AC_H_HYST; }
// 土壤越界/回安全(按端口双模式:raw=true 为原始值口 0~1023 低=湿高=干):
// 两态口 0=干 1=湿;原始值口 ≥soilDry 判干 / ≤soilWet 判湿,之间保持(滞回)。
// s<0(无数据)恒返回 false:既不触发也不解除,维持现状。
bool overSoil(const Config &c, bool raw, int16_t s) {
  return s >= 0 && (raw ? s >= c.soilDry : s == 0);
}
bool safeSoil(const Config &c, bool raw, int16_t s) {
  return s >= 0 && (raw ? s <= c.soilWet : s == 1);
}

// 大棚(舵机)越界/回安全区(滞回用 AC_OVER_FACTOR)
bool overGh(const Config &c, float t, float co2, float h) {
  return (overTemp(c, t) && t >= c.tHigh * AC_OVER_FACTOR) ||
         (overCo2(c, co2) && co2 >= c.co2High * AC_OVER_FACTOR) ||
         (overHum(c, h) && h >= c.hHigh * AC_OVER_FACTOR);
}
// 大棚关闭需三者都回落到各自阈值以内(带一点余量防抖:≤阈值)
bool safeGh(const Config &c, float t, float co2, float h) {
  bool tIn = isnan(t)   || t   <= c.tHigh;
  bool cIn = isnan(co2) || co2 <= c.co2High;
  bool hIn = isnan(h)   || h   <= c.hHigh;
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
void updateGh(PortAuto &p, bool over, bool safe) {
  if (over) { p.ghHiCnt++; p.ghLoCnt = 0; }
  else if (safe) { p.ghLoCnt++; p.ghHiCnt = 0; }
  else return;
  if (!p.ghActive && p.ghHiCnt >= AC_CONFIRM_N) p.ghActive = true;
  else if (p.ghActive && p.ghLoCnt >= AC_CONFIRM_N) p.ghActive = false;
}

void sendCmd(uint8_t idx, const char* cmd) {
  port_service::send(idx, String(cmd));
}

}  // namespace

void begin() {
  // NVS 打开失败则全部使用 config.h 默认值
  if (s_prefs.begin("autocfg", false)) {
    for (uint8_t i = 0; i < PORT_COUNT; i++) loadConfig(i);
    s_prefs.end();
  }
  for (auto &p : s_pa) {
    for (auto &r : p.rules) { r.hiCnt = r.loCnt = 0; r.active = false; }
    p.ghHiCnt = p.ghLoCnt = 0;
    p.ghActive = false;
    p.pumpActive = false;
    p.fanDir = 0;
    p.lastSampleMs = 0;
  }
}

void loop() {
  uint32_t now = millis();
  for (uint8_t idx = 0; idx < PORT_COUNT; idx++) {
    PortAuto &p = s_pa[idx];
    if (now - p.lastSampleMs < AC_SAMPLE_MS) continue;
    p.lastSampleMs = now;

    if (!p.cfg.enabled) {               // 手动模式:规则不动作(保持现状)
      continue;
    }

    PortSnapshot s = port_service::snapshot(idx);
    if (!s.hasData) continue;           // 该端口尚无传感数据

    updateRule(p.rules[R_TEMP], overTemp(p.cfg, s.t), safeTemp(p.cfg, s.t));
    updateRule(p.rules[R_CO2],  overCo2(p.cfg, s.co2), safeCo2(p.cfg, s.co2));
    updateRule(p.rules[R_HUMH], overHum(p.cfg, s.h),   safeHum(p.cfg, s.h));
    updateRule(p.rules[R_SOIL], overSoil(p.cfg, PORT_SOIL_RAW[idx], s.soil),
                                safeSoil(p.cfg, PORT_SOIL_RAW[idx], s.soil));
    updateGh(p, overGh(p.cfg, s.t, s.co2, s.h), safeGh(p.cfg, s.t, s.co2, s.h));

    // ---- 期望状态(由规则聚合) ----
    bool heat = p.rules[R_TEMP].active || p.rules[R_CO2].active;  // 散热
    bool wet  = p.rules[R_HUMH].active;                            // 排湿
    p.pumpActive = p.rules[R_SOIL].active;

    // 风扇方向:高温/高CO₂ 散热优先 → 正转;否则高湿排湿 → 反转;否则停
    if (heat)        p.fanDir = 1;
    else if (wet)    p.fanDir = 2;
    else             p.fanDir = 0;

    // ---- 每周期「期望 vs 该端口从机实际回报」校正(不只翻转时下发) ----
    // 覆盖手动篡改/丢包/自动↔手动切换导致的失配;失配持续则每周期补发(自愈)。
    if (s.online && s.hasData) {
      if (s.fan != p.fanDir) {
        if (p.fanDir == 1) sendCmd(idx, "FAN FWD");
        else if (p.fanDir == 2) sendCmd(idx, "FAN REV");
        else sendCmd(idx, "FAN STOP");
      }
      uint8_t wantPump = p.pumpActive ? 1 : 0;
      if (s.pump != wantPump) sendCmd(idx, p.pumpActive ? "PUMP ON" : "PUMP OFF");
      uint8_t wantServo = p.ghActive ? 180 : 0;
      if (s.servo != wantServo) sendCmd(idx, p.ghActive ? "SERVO 180" : "SERVO 0");
    }
  }
}

Config getConfig(uint8_t idx) {
  if (idx >= PORT_COUNT) return Config();
  return s_pa[idx].cfg;
}

void setConfig(uint8_t idx, const Config &c) {
  if (idx >= PORT_COUNT) return;
  s_pa[idx].cfg = c;
  if (s_prefs.begin("autocfg", false)) {
    char k[10];
    snprintf(k, sizeof(k), "en%u", idx);     s_prefs.putBool(k, c.enabled);
    snprintf(k, sizeof(k), "tHi%u", idx);    s_prefs.putFloat(k, c.tHigh);
    snprintf(k, sizeof(k), "co2Hi%u", idx);  s_prefs.putFloat(k, c.co2High);
    snprintf(k, sizeof(k), "hHi%u", idx);    s_prefs.putFloat(k, c.hHigh);
    snprintf(k, sizeof(k), "sWt%u", idx);    s_prefs.putFloat(k, c.soilWet);
    snprintf(k, sizeof(k), "sDr%u", idx);    s_prefs.putFloat(k, c.soilDry);
    s_prefs.end();
  }
}

uint8_t fanDir(uint8_t idx)   { return idx < PORT_COUNT ? s_pa[idx].fanDir : 0; }
bool pumpActive(uint8_t idx)  { return idx < PORT_COUNT ? s_pa[idx].pumpActive : false; }
bool servoActive(uint8_t idx) { return idx < PORT_COUNT ? s_pa[idx].ghActive : false; }

uint8_t ruleCount() { return R_COUNT; }

RuleState ruleState(uint8_t idx, uint8_t r) {
  RuleState st;
  st.name = (r < R_COUNT) ? RULE_NAMES[r] : "?";
  if (idx >= PORT_COUNT || r >= R_COUNT) return st;
  st.active = s_pa[idx].rules[r].active;
  return st;
}

}  // namespace auto_ctrl
