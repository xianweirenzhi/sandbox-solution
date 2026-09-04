#pragma once
// ================= 全局配置 =================
// WiFi 名称/密码、超时时间等参数统一在此修改,其他文件只引用不定义

#include <Arduino.h>
#include <IPAddress.h>

// ---------- STA:要连接的路由器/热点 ----------
#define STA_SSID        "ESP-TEST"     // 目标 AP 的 SSID
#define STA_PASS        "88888888"     // 目标 AP 的密码
#define STA_TIMEOUT_MS  120000UL       // 2 分钟内连不上 -> 自动切换 AP 模式

// ---------- AP 兜底模式(路由器不可用时手机直连设备) ----------
#define FALLBACK_AP_SSID  "ESP32-Direct"
#define FALLBACK_AP_PASS  "88888888"
#define FALLBACK_AP_IP    IPAddress(192, 168, 4, 1)   // 设备 IP,连热点后浏览器访问
#define FALLBACK_AP_MASK  IPAddress(255, 255, 255, 0)

// ---------- 网页服务 ----------
#define WEB_PORT   80          // HTTP 服务端口
#define MDNS_HOST  "esp32s3"   // 支持的设备可用 http://esp32s3.local 访问

// ---------- 串口 ----------
#define SERIAL_BAUD 115200
