#pragma once

#include <Arduino.h>
#include "config.h"

// 阈值自动控制模块(主机端):**每个从机端口独立一套**智能大棚控制。
// 规则:高温→风扇正转 / 高CO₂→风扇正转 / 高湿→风扇反转排湿(正转优先)/
//      土壤干→水泵;温/CO₂/高湿任一超阈 AC_OVER_PCT% 开大棚(舵机 180°),全回阈内关棚(0°)。
// 土壤判定按端口双模式(config.h PORT_SOIL_PCT):数字两态口 0=干/1=湿;
// 百分比口(如 F8266-2)读数 0~100 低=湿高=干,≥soilDry 判干/≤soilWet 判湿,之间保持(滞回)。
// 滞回 + 连续 N 次确认防抖;每周期以「期望 vs 该端口从机实际回报」校正(手动篡改/切档失配自动恢复)。
// 配置(总开关 + 5 阈值)按端口序号存 NVS(键名带后缀 en0/tHi0/...),网页可逐端口改。
namespace auto_ctrl {

// 运行期配置(网页可读改)
struct Config {
  bool  enabled = true;    // 自动控制总开关
  float tHigh   = 0.0f;    // 高温触发 ℃(风扇正转散热)
  float co2High = 0.0f;    // 高 CO₂ 触发 ppm(风扇正转散热)
  float hHigh   = 0.0f;    // 高湿触发 %(风扇反转排湿)
  float soilWet = 0.0f;    // 土壤湿阈值 %(仅百分比端口:读数≤此值=湿润解除浇水)
  float soilDry = 0.0f;    // 土壤干阈值 %(仅百分比端口:读数≥此值=过干触发浇水)
};

// 单条规则实时状态(网页展示用;传感值由 /api/ports 提供,此处只需触发态)
struct RuleState {
  const char* name;     // 规则名
  bool  active = false; // 是否已触发
};

void begin();                            // 载入全部端口 NVS 配置(无则用默认),重置规则状态
void loop();                             // 逐端口周期判断 + 下发命令(需主循环调用)
Config getConfig(uint8_t idx);           // 读第 idx 个端口(0~PORT_COUNT-1)的配置
void setConfig(uint8_t idx, const Config &c);  // 写指定端口配置并持久化到 NVS
uint8_t fanDir(uint8_t idx);             // 风扇期望方向 0=停 1=正转 2=反转
bool pumpActive(uint8_t idx);            // 水泵聚合状态(土壤干浇水)
bool servoActive(uint8_t idx);           // 大棚是否应开(舵机 180°)
uint8_t ruleCount();                     // 规则条数(4:高温/高CO₂/高湿/土壤)
RuleState ruleState(uint8_t idx, uint8_t r);   // 取第 idx 端口第 r 条规则状态

}  // namespace auto_ctrl
