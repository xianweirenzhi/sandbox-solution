#pragma once

#include <Arduino.h>

// 阈值自动控制模块(主机端):智能大棚控制。
// 规则:高温→风扇正转 / 高CO₂→风扇正转 / 高湿→风扇反转排湿(正转优先)/
//      土壤干→水泵;温/CO₂/高湿任一超阈 AC_OVER_PCT% 开大棚(舵机 180°),全回阈内关棚(0°)。
// 滞回 + 连续 N 次确认防抖;每周期以「期望 vs 从机实际回报」校正(手动篡改/切档失配自动恢复)。
// 配置(总开关 + 3 阈值)存 NVS,网页可改。
namespace auto_ctrl {

// 运行期配置(网页可读改)
struct Config {
  bool  enabled = true;    // 自动控制总开关
  float tHigh   = 0.0f;    // 高温触发 ℃(风扇正转散热)
  float co2High = 0.0f;    // 高 CO₂ 触发 ppm(风扇正转散热)
  float hHigh   = 0.0f;    // 高湿触发 %(风扇反转排湿)
};

// 单条规则实时状态(网页展示用;传感值由 /api/ports 提供,此处只需触发态)
struct RuleState {
  const char* name;     // 规则名
  bool  active = false; // 是否已触发
};

void begin();                     // 载入 NVS 配置(无则用默认),重置规则状态
void loop();                      // 周期判断 + 下发命令(需主循环调用)
Config getConfig();               // 读当前配置
void setConfig(const Config &c);  // 写配置并持久化到 NVS
bool fanActive();                 // 风扇是否应转(正转/反转任一)
uint8_t fanDir();                 // 风扇期望方向 0=停 1=正转 2=反转
bool pumpActive();                // 水泵聚合状态(土壤干浇水)
bool servoActive();               // 大棚是否应开(舵机 180°)
uint8_t ruleCount();              // 规则条数(4:高温/高CO₂/高湿/土壤)
RuleState ruleState(uint8_t idx); // 取第 idx 条规则状态

}  // namespace auto_ctrl
