# ESP32-S3 STA/AP 双模式设备状态服务

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-04

## 1. 项目简介

ESP32-S3 设备接入项目：优先以 **STA 模式**连接路由器；若 2 分钟内无法连接，**自动切换 AP 直连兜底模式**。两种模式下均提供网页服务，局域网/直连用户通过浏览器访问设备 IP 即可查看设备实时状态（网络信息 + 系统信息，每 2 秒自动刷新）。

本项目位于仓库 [sandbox-solution](https://github.com/xianweirenzhi/sandbox-solution)（公开）的 `esp32-1/` 子目录——仓库为多板卡项目集合，其他板卡项目以同级目录存放，详见仓库根 [README](../README.md)。硬件：4D Systems gen4-ESP32S3 (R8N16)，框架：Arduino (PlatformIO)。

## 2. 功能特性

- [x] STA 模式直连指定路由器（SSID/PASSWORD 见 `config.h`），串口输出连接验证信息
- [x] 2 分钟连接超时后自动切换 AP 模式（热点 `ESP32-Direct`），手机直连可访问设备网页
- [x] 设备状态网页：工作模式、SSID、IP、网关、子网掩码、MAC、RSSI（含质量评价）、信道
- [x] 系统信息：运行时间、堆内存、PSRAM、Flash、芯片型号/核心数、CPU 频率、核心温度、SDK 版本
- [x] `/api/status` JSON 接口，页面 2 秒自动刷新
- [x] mDNS：支持 `http://esp32s3.local` 访问（部分设备/浏览器支持）
- [x] 串口 5 秒心跳回报（AP 模式下含已连接客户端数量）

## 3. 开发环境

| 项 | 值 |
|---|---|
| IDE | VSCode + PlatformIO |
| 平台 | espressif32 |
| 开发板 | `4d_systems_esp32s3_gen4_r8n16`（ESP32-S3, 16MB Flash / 8MB PSRAM） |
| 框架 | Arduino |
| 串口 | 115200，USB CDC（板定义已内置，无需额外编译标志） |
| 外部库 | 无（仅使用核心自带 WiFi / WebServer / ESPmDNS） |

## 4. 目录结构

```
F_C_S3/
├── platformio.ini        PlatformIO 工程配置
├── README.md             本文档
└── src/
    ├── main.cpp          入口：流程编排 + 串口连接信息打印 + 心跳
    ├── config.h          ★ 全部可调参数（WiFi、超时、AP 网段、端口）
    ├── wifi_service.h/.cpp   WiFi 管理：STA 优先 → 超时切 AP（对外暴露 mode/staConnected）
    ├── device_status.h/.cpp 状态采集：collectStatus() 返回 DeviceStatus 结构体
    └── web_ui.h/.cpp     网页服务：页面 HTML、路由注册、/api/status JSON
```

**分层依赖**：`main` → `wifi_service` / `web_ui` / `device_status`；`web_ui`、`device_status` → `wifi_service`（读取当前模式）；参数统一来自 `config.h`。

## 5. 快速开始

1. 用 VSCode 打开项目文件夹，PlatformIO 会自动识别 `platformio.ini`
2. 修改 `src/config.h` 中的 `STA_SSID` / `STA_PASS` 为实际路由器/热点凭据
3. 点击 **Upload**（→）编译烧录
4. 打开 **Serial Monitor**（波特率 115200）观察连接过程
5. 连接成功后按串口提示的 URL 用浏览器访问：

```
[Web] 网页服务已启动: http://192.168.4.23/
[Web] 部分设备也可用: http://esp32s3.local/
```

命令行方式：

```bash
pio run                # 编译
pio run -t upload      # 烧录
pio device monitor     # 串口监视
```

## 6. 配置说明（config.h）

| 宏 | 默认值 | 说明 |
|---|---|---|
| `STA_SSID` | `"ESP-TEST"` | 要连接的路由器/热点 SSID |
| `STA_PASS` | `"88888888"` | 对应密码 |
| `STA_TIMEOUT_MS` | `120000` | STA 连接超时（2 分钟），超时切 AP 模式 |
| `FALLBACK_AP_SSID` | `"ESP32-Direct"` | 兜底热点名称 |
| `FALLBACK_AP_PASS` | `"88888888"` | 兜底热点密码 |
| `FALLBACK_AP_IP` | `192.168.4.1` | AP 模式设备 IP（网页访问地址） |
| `FALLBACK_AP_MASK` | `255.255.255.0` | AP 模式子网掩码 |
| `WEB_PORT` | `80` | HTTP 服务端口 |
| `MDNS_HOST` | `"esp32s3"` | mDNS 主机名（`esp32s3.local`） |
| `SERIAL_BAUD` | `115200` | 串口波特率 |

> 若路由器网段也是 `192.168.4.x`，请把 `FALLBACK_AP_IP/MASK` 改为其他网段（如 `192.168.5.1`）避免冲突。

## 7. 运行逻辑

```
上电
 │
 ▼
[1] STA 模式连接 STA_SSID（最长 STA_TIMEOUT_MS，串口每 10s 打印剩余时间）
 │
 ├── 成功 ──► STA 模式运行：网页服务监听路由器分配的 IP
 │            网页徽章：绿色「在线」；掉线由 WiFi 自动重连恢复
 │
 └── 超时 ──► [2] 切换纯 AP 模式：热点 FALLBACK_AP_SSID / FALLBACK_AP_PASS
              设备 IP 固定为 FALLBACK_AP_IP，手机直连热点后访问该 IP
              网页徽章：橙色「AP 直连模式」
              ※ AP 模式为终态，重启设备才会重新尝试 STA
```

## 8. 串口输出示例

**STA 模式成功：**

```
===== ESP32-S3 STA/AP 双模式设备 =====
[WiFi] 正在连接路由器 "ESP-TEST"(最长等待 120s)...
[WiFi] 连接中... 剩余 110s          ← 仅在等待时出现

========= WiFi STA 连接成功 =========
模式     : STA
SSID     : ESP-TEST
IP 地址  : 192.168.4.23
...
[Web] 网页服务已启动: http://192.168.4.23/
[在线] SSID:ESP-TEST  IP:192.168.4.23  RSSI:-48 dBm  信道:6
```

**AP 兜底模式（路由器 2 分钟连不上）：**

```
[WiFi] 120s 内未能连接 "ESP-TEST",已切换 AP 直连模式

========= AP 直连兜底模式 =========
热点名称 : ESP32-Direct
热点密码 : 88888888
设备 IP  : 192.168.4.1
...
[Web] 网页服务已启动: http://192.168.4.1/
[AP] 热点:ESP32-Direct  IP:192.168.4.1  已连接设备:1 台
```

## 9. Web 接口文档

| 路由 | 方法 | 说明 |
|---|---|---|
| `/` | GET | 状态页面（HTML，存于 Flash/PROGMEM） |
| `/api/status` | GET | 设备状态 JSON |
| 其他 | — | 返回 404 |

**`/api/status` 返回字段：**

| 字段 | 类型 | 说明 |
|---|---|---|
| `mode` | string | `"STA"`（已连路由器）/ `"AP"`（兜底直连模式） |
| `connected` | bool | 是否已连上路由器（AP 模式恒为 `false`） |
| `ssid` | string | STA：关联的 AP 名；AP：热点名 |
| `ip` / `gw` / `mask` | string | 本机 IP / 网关 / 子网掩码 |
| `mac` | string | STA MAC 地址 |
| `rssi` | number | 信号强度 dBm（AP 模式为 0，页面显示 —） |
| `ch` | number | 信道（AP 模式为 0，页面显示 —） |
| `host` | string | 主机名 |
| `uptime` | string | 运行时间，如 `0d 00:12:34` |
| `heapFree` / `heapSize` | number | 可用/总堆内存（KB） |
| `psramFree` / `psramSize` | number | 可用/总 PSRAM（KB） |
| `flashMB` / `flashMHz` | number | Flash 容量（MB）/ 频率（MHz） |
| `chip` | string | 芯片型号 rev 版本 xN 核 |
| `cpuMHz` | number | CPU 频率 |
| `tempC` | string | 核心温度（近似值） |
| `sdk` | string | ESP-IDF SDK 版本 |

## 10. 测试验证场景

| # | 场景 | 操作 | 预期 |
|---|---|---|---|
| 1 | STA 正常连接 | 开启 `ESP-TEST` 热点后上电 | 约 5~10s 连上，串口打印 IP，浏览器经同网 WiFi 访问该 IP 出现绿色「在线」状态页 |
| 2 | AP 兜底切换 | 关闭 `ESP-TEST` 后上电 | 串口每 10s 报剩余时间，2 分钟后切换 AP；手机连 `ESP32-Direct`，访问 `192.168.4.1` 出现橙色「AP 直连模式」页面 |
| 3 | 掉线恢复 | STA 模式下临时关闭路由器 | 串口出现 `[离线]`，路由器恢复后自动重连（无需重启） |
| 4 | 串口无输出 | — | 检查串口波特率为 115200；本板使用 USB CDC，换 USB 线/口重试 |

## 11. 二次开发指南

### 新增一个网页接口/控制功能（以 LED 控制为例）

1. `web_ui.cpp` 中注册路由：

```cpp
server.on("/api/led", HTTP_GET, []() {
  // 示例：http://<ip>/api/led?state=on
  String state = server.arg("state");
  // digitalWrite(LED_PIN, state == "on" ? HIGH : LOW);
  server.send(200, "application/json", "{\"ok\":true}");
});
```

2. 需要新数据项时：在 `DeviceStatus` 结构体加字段 → `device_status.cpp` 的 `collectStatus()` 采集 → `handleStatus()` 序列化进 JSON → 页面 `INDEX_HTML` 加一行表格并在 `refresh()` 里赋值

### 新增独立功能模块

按现有模式扩展：新建 `xxx.h/.cpp`（命名空间风格对外暴露 `begin()/loop()`），在 `main.cpp` 中 `#include` 并在 `setup()/loop()` 调用即可，PlatformIO 会自动编译 `src/` 下所有 `.cpp`。

### 已预留的扩展点

- `wifi_service::loop()`：可加「AP 模式下周期后台重试 STA、成功后自动切回」
- `web_ui.cpp` 路由注册处：可加 WiFi 改配表单、设备重启按钮等
- `config.h`：所有参数集中于此，改完重新编译烧录即生效

## 12. 常见问题（FAQ）

- **手机连路由器 WiFi 但打不开设备页面**：部分手机热点有 AP 隔离，限制设备互访；换路由器或电脑开热点验证
- **`esp32s3.local` 打不开**：mDNS 依赖系统/浏览器支持，Windows 10+、iOS、macOS 一般可用，Android 常不支持，此时直接用 IP 访问
- **AP 模式下手机显示「无法访问互联网」**：正常现象（设备热点本来就不提供外网），选择仍然连接即可
- **WiFiManager 库去哪了**：AP 兜底 + 自有状态页已覆盖其门户功能，为保持精简已移除依赖；如需「网页改配 WiFi」可重新引入做按需门户

## 13. 维护约定

> **改动代码时同步更新本文档对应章节**，保持文档与代码一致：

- 改参数/默认值 → 更新 §6 配置表
- 改模式切换逻辑/超时行为 → 更新 §7 运行逻辑
- 增删路由或 JSON 字段 → 更新 §9 接口文档
- 新增文件/模块 → 更新 §4 目录结构
- 每次同步更新文档版本日期
