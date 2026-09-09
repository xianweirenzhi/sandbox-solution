#pragma once

#include <Arduino.h>

// 设备状态快照:网页 JSON 与串口输出共用一份数据源
struct DeviceStatus {
  // ---- 网络 ----
  String  mode;                 // "STA" / "AP"
  bool    connected = false;    // 是否已连上路由器(AP 模式恒为 false)
  String  ssid;                 // STA:关联的 AP 名 / AP:热点名
  String  ip, gw, mask;         // 本机 IP、网关、子网掩码
  String  mac;                  // STA MAC 地址
  int32_t rssi = 0;             // 信号强度(AP 模式无效,置 0)
  uint8_t channel = 0;          // 信道(AP 模式无效,置 0)
  String  hostname;

  // ---- 系统 ----
  String   uptime;              // 运行时间 "Xd HH:MM:SS"
  uint32_t heapFree = 0;        // 可用堆内存 (KB)
  uint32_t heapSize = 0;        // 堆内存总量 (KB)
  uint32_t psramFree = 0;       // 可用 PSRAM (KB)
  uint32_t psramSize = 0;       // PSRAM 总量 (KB)
  uint32_t flashMB = 0;         // Flash 大小 (MB)
  uint32_t flashMHz = 0;        // Flash 频率 (MHz)
  String   chip;                // 型号 rev版本 xN核
  uint32_t cpuMHz = 0;          // CPU 频率
  String   tempC;               // 核心温度(近似)
  String   sdk;                 // SDK 版本
};

// 采集一次当前设备状态
DeviceStatus collectStatus();
