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

// ===================== 主机（esp32-1）TCP 通信参数 =====================
// 本从机(F8266-1)作为 TCP 客户端连主机 NET_HOST_IP:NET_HOST_PORT，连上即上报上线。
// ★ 主机与从机须处于同一网段（默认网络所在 LAN），改主机 IP / 端口只改此处。
// 说明：TCP 连接为同步式，主机离线时 SDK 内部会在数秒内超时返回，由下方重试间隔限频。
#define NET_HOST_IP      "192.168.1.100"  // 主机回退 IP（动态发现失败时用；发现成功自动覆盖）
#define NET_HOST_PORT     8000             // 本从机(F8266-1)对应主机的监听端口
#define NET_LINK_RETRY_MS 3000UL           // 连接失败 / 断线后的重试间隔（毫秒）

// ===================== 主机动态发现（esp32-1 UDP 广播）=====================
// 主机可能是 DHCP 动态 IP，会周期向局域网 UDP 广播宣告（载荷 ESP32HOST,<ip>,<base>,<count>，
// 无 @ 帧头；与主机 host_announce 的 HOST_ANNOUNCE_PREFIX 一致）。
// 从机监听 NET_HOST_ANNOUNCE_PORT 即可动态获知主机 IP，无需写死；收到有效宣告自动切到
// 发现地址；若超过 NET_HOST_DISCOVER_STALE_MS 再无宣告（主机离线/换网）则回退 NET_HOST_IP。
#define NET_HOST_ANNOUNCE_PORT       45555    // 与主机 HOST_ANNOUNCE_PORT 一致
#define NET_HOST_ANNOUNCE_PREFIX     "ESP32HOST"
#define NET_HOST_DISCOVER_STALE_MS   20000UL  // 超过此时长未收到宣告则回退硬编码 IP

// 单条命令帧【内容】的最大长度（字符数，不含 @ 与 /）。超过上限的帧整帧丢弃，
// 防止畸形/超长数据拖垮 RAM 受限的 ESP8266（仅 80KB RAM）；确需更大请调此处后重编译。
#define NET_FRAME_MAX_LEN 64
