#pragma once

// ===================== F8266-1 网络配置（集中在此修改）=====================
// 设备统一命名 F8266-1：STA 主机名与配置热点名均取 NET_DEVICE_NAME。
// 需要调整无线参数 / 名称时只改本文件，业务代码无需变动。

// 开机优先直连的默认无线网络（硬编码默认；连不上时自动打开同名配置热点兜底）
#define NET_DEFAULT_SSID       "ESP-TEST"
#define NET_DEFAULT_PASSWORD   "88888888"

// 设备统一名称：STA 主机名（路由器 / 主机端显示的名字）与配置热点名共用
#define NET_DEVICE_NAME        "F8266-1"

// 直连默认网络的等待时长（毫秒）
#define NET_CONNECT_TIMEOUT_MS 12000UL
// 配置热点最大停留时长（秒）；超时仍无人配置则自动重启重试
#define NET_PORTAL_TIMEOUT_S   180
