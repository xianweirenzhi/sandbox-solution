#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

// 单个通信端口的状态快照(网页展示用)
struct PortSnapshot {
  uint16_t port = 0;
  uint8_t  clients = 0;          // 当前 TCP 连接数
  bool     online = false;       // 从机是否已上报上线("F8266-x online")且连接保持
  String   slave;                // 已上线的从机名称,如 "F8266-1"
  uint32_t rxCount = 0;          // 累计接收报文数
  uint32_t txCount = 0;          // 累计发送次数
  String   log[PORT_LOG_LINES];  // 最近收发记录(按时间序)
  uint8_t  logLen = 0;
  // 从机上报的最新传感数据(JSON 帧解析结果;NaN/负值表示无数据)
  bool     hasData = false;      // 是否收到过传感数据帧
  float    t = 0.0f;             // 温度 ℃(NaN=无)
  float    h = 0.0f;             // 湿度 %(NaN=无)
  float    lux = 0.0f;           // 光照 lx(NaN=无)
  int16_t  soil = -1;            // 土壤 -1=无;两态口 0=干/1=湿,原始值口 0~1023 低=湿高=干(模式见 config.h PORT_SOIL_RAW)
  float    co2 = 0.0f;           // eCO₂ ppm(NaN=无)
  float    tvoc = 0.0f;          // TVOC ppb(NaN=无)
  // 从机上报的执行器实时状态
  uint8_t  pump = 0;             // 水泵 0=关 1=开
  uint8_t  fan = 0;              // 风扇 0=OFF 1=FWD 2=REV
  uint8_t  servo = 90;           // 舵机角度 0~180
};

// 从机通信服务:开启 PORT_COUNT 个连续 TCP 端口,提供收发与状态查询
namespace port_service {

void begin();                                // 开启全部端口监听
void loop();                                 // 接入/收包/断开检测,需在主循环中调用
bool send(uint8_t idx, const String &text);  // 向第 idx 个端口的所有从机发送(自动补换行)
PortSnapshot snapshot(uint8_t idx);          // 获取端口状态快照

}  // namespace port_service
