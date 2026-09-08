#pragma once
// ================= 全局配置 =================
// WiFi 名称/密码、超时时间等参数统一在此修改,其他文件只引用不定义

#include <Arduino.h>
#include <IPAddress.h>

// ---------- STA:要连接的路由器/热点 ----------
#define STA_SSID        "ESP-TEST"     // 目标 AP 的 SSID
#define STA_PASS        "88888888"     // 目标 AP 的密码
#define STA_TIMEOUT_MS  120000UL       // 2 分钟内连不上 -> 自动切换 AP 模式

// ---------- STA IP 获取方式 ----------
// 0 = DHCP(由路由器自动分配,默认;IP 可能变化,以串口/网页显示为准)
// 1 = 固定 IP(便于从机定位主机,须与路由器同网段,网关填路由器 IP)
#define STA_USE_STATIC_IP  0

#if STA_USE_STATIC_IP
#define STA_STATIC_IP   IPAddress(192, 168, 4, 200)   // 本机固定 IP
#define STA_GATEWAY     IPAddress(192, 168, 4, 1)     // 网关(路由器 IP)
#define STA_NETMASK     IPAddress(255, 255, 255, 0)
#define STA_DNS         IPAddress(192, 168, 4, 1)     // DNS(与网关相同即可)
#endif

// ---------- 从机通信 TCP 端口 ----------
#define PORT_BASE        8000   // 起始端口(共 PORT_COUNT 个连续端口: 8000~8005)
#define PORT_COUNT       6      // 端口数量
#define PORT_CLIENTS_MAX 3      // 每端口允许接入的最大从机数
#define PORT_LOG_LINES   8      // 每端口在网页上保留的收发记录条数
#define PORT_RX_TIMEOUT_MS 200  // 不完整帧(缺帧尾'/')的静默超时兜底(ms)

// ---------- TCP keepalive(感知从机异常掉电,仅主机端即可生效) ----------
// 从机掉电不发送 FIN,连接停留在半开状态;由 lwIP keepalive 在空闲
// IDLE 秒后按 INTVL 秒探测,连续 CNT 次无响应则中止连接 -> recv 报错 -> 判离线。
// 判定用时约 IDLE + INTVL*CNT(默认 3+2*3 ≈ 最快 9s)。
#define PORT_KEEPALIVE_IDLE_S   3   // 空闲多久开始探测(秒)
#define PORT_KEEPALIVE_INTVL_S  2   // 探测间隔(秒)
#define PORT_KEEPALIVE_CNT      3   // 无响应探测次数

// ---------- 板载 RGB 灯(NeoPixel/WS2812,GPIO48) ----------
#define RGB_LED_PIN  48   // 板载 NeoPixel 数据线,当前仅拉低保持熄灭

// ---------- 主机 UDP 广播宣告(供从机 F8266-x 动态发现主机 IP) ----------
#define HOST_ANNOUNCE_PORT        45555    // UDP 端口(主机与从机须一致)
#define HOST_ANNOUNCE_INTERVAL_MS 3000UL   // 广播间隔(ms)
#define HOST_ANNOUNCE_PREFIX      "ESP32HOST"  // 载荷前缀: ESP32HOST,<ip>,<base>,<count>

// ---------- 阈值自动控制(网页可改,存 NVS 覆盖以下默认) ----------
// 高温→风扇 / 高CO₂→风扇 / 土壤干→水泵 / 低湿→水泵;滞回 + 连续 N 次确认防抖
#define AC_T_HIGH      30.0f     // 高温触发阈值 ℃
#define AC_T_HYST      2.0f      // 温度滞回 ℃(固定,不暴露网页)
#define AC_CO2_HIGH    1200.0f   // 高 CO₂ 触发阈值 ppm
#define AC_CO2_HYST    200.0f    // CO₂ 滞回 ppm(固定)
#define AC_H_LOW       40.0f     // 低湿触发阈值 %
#define AC_H_HYST      5.0f      // 湿度滞回 %(固定)
#define AC_CONFIRM_N   3         // 连续越界/回安全区次数(约 6s,2s 一次上报)
#define AC_SAMPLE_MS   2000UL    // 自动判断周期(与从机上报周期对齐)

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
