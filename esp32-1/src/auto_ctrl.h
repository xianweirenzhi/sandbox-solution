#pragma once

#include <Arduino.h>

// 阈值自动控制模块(主机端):读取从机传感数据,越界自动下发执行器命令。
// 规则:高温→风扇 / 高CO₂→风扇 / 土壤干→水泵 / 低湿→水泵;滞回 + 连续 N 次确认防抖。
// 配置(总开关 + 3 阈值)存 NVS,网页可改。
namespace auto_ctrl {

// 运行期配置(网页可读改)
struct Config {
  bool  enabled = true;    // 自动控制总开关
  float tHigh   = 0.0f;    // 高温触发 ℃
  float co2High = 0.0f;    // 高 CO₂ 触发 ppm
  float hLow    = 0.0f;    // 低湿触发 %
};

// 单条规则实时状态(网页展示用;传感值由 /api/ports 提供,此处只需触发态)
struct RuleState {
  const char* name;     // 规则名
  bool  active = false; // 是否已触发
};

void begin();                     // 载入 NVS 配置(无则用默认),重置规则状态
void loop();                      // 周期判断 + 状态翻转时下发命令(需主循环调用)
Config getConfig();               // 读当前配置
void setConfig(const Config &c);  // 写配置并持久化到 NVS
bool fanActive();                 // 风扇聚合状态(高温‖CO₂)
bool pumpActive();                // 水泵聚合状态(土壤‖低湿)
uint8_t ruleCount();              // 规则条数(4)
RuleState ruleState(uint8_t idx); // 取第 idx 条规则状态

}  // namespace auto_ctrl
