#pragma once
// ================= 全局配置 =================
// WiFi 名称/密码、超时时间等参数统一在此修改,其他文件只引用不定义

#include <Arduino.h>
#include <IPAddress.h>

// ---------- STA:要连接的路由器/热点 ----------
#define STA_SSID        "ESP-TEST"     // 目标 AP 的 SSID
#define STA_PASS        "88888888"     // 目标 AP 的密码
#define STA_TIMEOUT_MS  120000UL       // 2 分钟内连不上 -> 自动切换 AP 模式

// ---------- STA 固定 IP(便于从机定位主机地址) ----------
// !!! 须与路由器同网段,网关填路由器 IP,不同环境请修改 !!!
#define STA_STATIC_IP   IPAddress(192, 168, 4, 200)   // 本机固定 IP
#define STA_GATEWAY     IPAddress(192, 168, 4, 1)     // 网关(路由器 IP)
#define STA_NETMASK     IPAddress(255, 255, 255, 0)
#define STA_DNS         IPAddress(192, 168, 4, 1)     // DNS(与网关相同即可)

// ---------- 从机通信 TCP 端口 ----------
#define PORT_BASE        8000   // 起始端口(共 PORT_COUNT 个连续端口: 8000~8005)
#define PORT_COUNT       6      // 端口数量
#define PORT_CLIENTS_MAX 3      // 每端口允许接入的最大从机数
#define PORT_LOG_LINES   8      // 每端口在网页上保留的收发记录条数

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
