# ESP32-S3 STA/AP 双模式设备状态服务

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-08

## 1. 项目简介

ESP32-S3 主机设备项目：优先以 **STA 模式**（默认 **DHCP** 自动获取 IP，可配置固定 IP）连接路由器；若 2 分钟内无法连接，**自动切换 AP 直连兜底模式**。作为从机通信的**主机**，开启 **6 个连续 TCP 端口**（8000~8005）与从机（F8266-x）通信。网页提供「精简设备状态卡 + 2×3 六端口收发窗口」，可实时查看各端口收发报文、从机上线状态，并直接向从机发送报文。

本项目位于仓库 [sandbox-solution](https://github.com/xianweirenzhi/sandbox-solution)（公开）的 `esp32-1/` 子目录——仓库为多板卡项目集合，其他板卡项目以同级目录存放，详见仓库根 [README](../README.md)。硬件：4D Systems gen4-ESP32S3 (R8N16)，框架：Arduino (PlatformIO)。

## 2. 功能特性

- [x] STA 模式直连指定路由器（默认 **DHCP** 自动获取 IP，可切换固定 IP，见 `config.h`），串口输出连接验证信息
- [x] 2 分钟连接超时后自动切换 AP 模式（热点 `ESP32-Direct`），手机直连可访问设备网页
- [x] 6 个连续 TCP 端口（8000~8005）作为主机监听从机连接，`@命令/` 帧式报文收发
- [x] 从机上线协议：收到 `@F8266-x online/` 后对应端口窗口标记「F8266-x 已上线」，断开自动标记离线
- [x] 网页：设备状态卡 + **传感数据卡（6 指标进度条图形化）** + **执行器控制按键（水泵/风扇/舵机）** + 二级日志页（原 2×3 收发窗口收进，点击「通信日志」展开）
- [x] `/api/status`、`/api/ports`（端口状态 + 最新传感数据）JSON 接口，`/api/send`（向端口发报文/执行器命令）
- [x] 主机 UDP 广播宣告：STA 在线时周期向局域网广播自身 IP/端口，**从机 F8266-x 可动态发现主机**（主机 DHCP 动态 IP 也能被找到）
- [x] **阈值自动控制**：4 条规则（高温→风扇 / 高CO₂→风扇 / 土壤干→水泵 / 低湿→水泵），滞回 + 连续 N 次确认防抖，越界自动下发执行器命令；网页可改阈值 + 自动/手动总开关，配置存 NVS 重启保留
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
    ├── config.h          ★ 全部可调参数（WiFi、IP 获取方式、超时、AP 网段、端口、广播宣告）
    ├── wifi_service.h/.cpp   WiFi 管理：STA 优先(DHCP/固定 IP) → 超时切 AP
    ├── device_status.h/.cpp 状态采集：collectStatus() 返回 DeviceStatus 结构体
    ├── port_service.h/.cpp  从机通信：6 个 TCP 端口收发、上线报文识别、传感 JSON 解析存储、TCP keepalive 掉电探测、日志环形缓冲
    ├── host_announce.h/.cpp 主机 UDP 广播宣告：周期广播自身 IP/端口，供从机动态发现
    ├── auto_ctrl.h/.cpp     阈值自动控制：4 规则滞回+防抖，越界自动下发执行器命令，配置存 NVS
    └── web_ui.h/.cpp     网页服务：页面 HTML（状态卡+数据卡+执行器+日志二级页）、路由注册、各 JSON 接口
```

**分层依赖**：`main` → `wifi_service` / `port_service` / `web_ui` / `device_status`；`web_ui`、`device_status` → `wifi_service`（读取当前模式）；`web_ui` → `port_service`（读取端口快照）；参数统一来自 `config.h`。

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
| `STA_USE_STATIC_IP` | `0` | IP 获取方式：`0`=DHCP 自动分配（默认），`1`=固定 IP |
| `STA_STATIC_IP` | `192.168.4.200` | 固定 IP（仅 `STA_USE_STATIC_IP=1` 时生效，**须与路由器同网段**） |
| `STA_GATEWAY` | `192.168.4.1` | 网关（路由器 IP，固定 IP 时生效） |
| `STA_NETMASK` | `255.255.255.0` | 子网掩码（固定 IP 时生效） |
| `STA_DNS` | `192.168.4.1` | DNS（与网关一致即可，固定 IP 时生效） |
| `PORT_BASE` | `8000` | 从机通信起始端口（监听 8000~8005） |
| `PORT_COUNT` | `6` | 端口数量（网页 2×3 卡片与之对应） |
| `PORT_CLIENTS_MAX` | `3` | 每端口最大接入从机数 |
| `PORT_LOG_LINES` | `8` | 每端口网页保留的收发记录条数 |
| `PORT_RX_TIMEOUT_MS` | `200` | 不完整帧（缺帧尾 `/`）的静默超时兜底（ms） |
| `PORT_KEEPALIVE_IDLE_S` | `3` | TCP keepalive 空闲探测阈值（秒），从机异常掉电约 `IDLE+INTVL*CNT` 秒内判定离线 |
| `PORT_KEEPALIVE_INTVL_S` | `2` | TCP keepalive 探测间隔（秒） |
| `PORT_KEEPALIVE_CNT` | `3` | TCP keepalive 无响应探测次数 |
| `RGB_LED_PIN` | `48` | 板载 NeoPixel RGB 灯数据线（当前仅拉低保持熄灭） |
| `FALLBACK_AP_SSID` | `"ESP32-Direct"` | 兜底热点名称 |
| `FALLBACK_AP_PASS` | `"88888888"` | 兜底热点密码 |
| `FALLBACK_AP_IP` | `192.168.4.1` | AP 模式设备 IP（网页访问地址） |
| `FALLBACK_AP_MASK` | `255.255.255.0` | AP 模式子网掩码 |
| `WEB_PORT` | `80` | HTTP 服务端口 |
| `MDNS_HOST` | `"esp32s3"` | mDNS 主机名（`esp32s3.local`） |
| `SERIAL_BAUD` | `115200` | 串口波特率 |
| `HOST_ANNOUNCE_PORT` | `45555` | 主机 UDP 广播端口（从机 `NET_HOST_ANNOUNCE_PORT` 须一致） |
| `HOST_ANNOUNCE_INTERVAL_MS` | `3000` | 广播间隔（ms） |
| `HOST_ANNOUNCE_PREFIX` | `"ESP32HOST"` | 宣告载荷前缀，格式 `ESP32HOST,<ip>,<base>,<count>` |
| `AC_T_HIGH` | `30.0` | 高温触发阈值 ℃（网页可改，存 NVS 覆盖） |
| `AC_T_HYST` | `2.0` | 温度滞回 ℃（固定，不暴露网页） |
| `AC_CO2_HIGH` | `1200` | 高 CO₂ 触发阈值 ppm |
| `AC_CO2_HYST` | `200` | CO₂ 滞回 ppm（固定） |
| `AC_H_LOW` | `40.0` | 低湿触发阈值 % |
| `AC_H_HYST` | `5.0` | 湿度滞回 %（固定） |
| `AC_CONFIRM_N` | `3` | 连续越界/回安全区次数（约 6s 防抖） |
| `AC_SAMPLE_MS` | `2000` | 自动判断周期（与从机上报周期对齐） |

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
[WiFi] IP 获取方式: DHCP(由路由器自动分配)
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
| `/` | GET | 主机控制台页面（HTML，存于 Flash/PROGMEM）：状态卡 + 传感数据卡（进度条）+ 执行器控制 + 二级日志页 |
| `/api/status` | GET | 设备状态 JSON |
| `/api/ports` | GET | 6 个从机通信端口状态 JSON（数组，含最新传感数据） |
| `/api/send` | POST | 向指定端口发送报文/命令，参数 `port`（端口号）、`text`（命令内容，自动包装为 `@内容/` 帧；执行器命令见下方词表） |
| `/api/auto` | GET | 返回自动控制配置（enabled/tHigh/co2High/hLow/fanActive/pumpActive）与 4 条规则触发状态 |
| `/api/auto` | POST | 保存自动控制配置，参数 `enabled`、`tHigh`、`co2High`、`hLow`，写入 NVS |
| 其他 | — | 返回 404 |

**`/api/ports` 返回字段（数组，每端口一项）：**

| 字段 | 类型 | 说明 |
|---|---|---|
| `port` | number | 端口号（8000~8005） |
| `clients` | number | 当前 TCP 连接数 |
| `online` | bool | 从机是否已上报上线且连接保持 |
| `slave` | string | 已上线从机名，如 `F8266-1` |
| `rx` / `tx` | number | 累计接收报文数 / 发送次数 |
| `hasData` | bool | 是否收到过传感数据帧 |
| `t` / `h` | number\|null | 温度 ℃ / 湿度 %（null=无数据） |
| `lux` | number\|null | 光照 lx |
| `soil` | number\|null | 土壤 0=干 / 1=湿（null=无数据） |
| `co2` / `tvoc` | number\|null | eCO₂ ppm / TVOC ppb |
| `pump` | number | 水泵实时状态 0=关 / 1=开（从机回报） |
| `fan` | number | 风扇实时状态 0=OFF / 1=正转 / 2=反转（从机回报） |
| `servo` | number | 舵机当前角度 0~180（从机回报） |
| `log` | string[] | 最近收发记录（时间序，最多 `PORT_LOG_LINES` 条） |

## 9.5 从机通信协议（帧格式）

- **连接**：从机作为 TCP 客户端连接 `主机IP:8000~8005` 中任一端口（每个端口最多 3 个连接）
- **帧格式**：`@` 为帧头，`/` 为帧尾，两者之间为**命令有效内容**；一条命令一帧，如 `@F8266-1 online/`
- **收发规则**：主机收到数据后按帧解析，只取 `@` 与 `/` 之间的内容作为命令；主机下发的命令也会自动包装为 `@命令/` 帧
- **上线**：从机连接后发送帧 `@F8266-x online/`（x 为从机编号），主机网页对应端口卡标记「**F8266-x 已上线**」（绿点），日志区出现 ★ 记录
- **数据上报**：从机每 2s 发 JSON 帧 `@{"t":25.3,"h":56.0,"lux":1234,"soil":1,"co2":800,"tvoc":12,"pump":0,"fan":1,"servo":90}/`，主机端以 `{` 开头识别并解析存储（ArduinoJson），`null` 字段表示该传感器无数据；`pump/fan/servo` 为执行器实时状态（主机据此在网页执行器卡显示当前态）；不逐条刷日志（高频），仅在首条打一条「开始接收传感数据」
- **下发命令（执行器控制）**：主机网页按键 / `/api/send` 下发 `PUMP ON/OFF`、`FAN FWD/REV/STOP`、`SERVO 0~180`，从机 `onHostCommand()` 解析执行
- **离线**：从机断开 TCP 连接后，端口卡自动标记离线（○ 记录）
- **异常掉电**：从机突然断电不会发 FIN，主机端通过 **TCP keepalive** 探测（空闲 3s 后每 2s 一次，3 次无响应中止连接），约 **5-10s** 内感知并标记离线——无需从机固件配合
- **主机动态发现**：主机 DHCP 动态 IP 时，主机每 3s 向局域网 UDP `45555` 广播 `ESP32HOST,<ip>,<base>,<count>`；从机监听同端口即可获知主机 IP（20s 无宣告则回退从机侧硬编码地址）
- **健壮性**：若帧缺少帧尾 `/`，静默超过 `PORT_RX_TIMEOUT_MS`（默认 200ms）后按不完整帧强制结束处理
- **测试**：可用网络调试助手（TCP Client 模式）连接主机 IP:8000，发送 `@F8266-1 online/` 验证

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
| 1 | STA 正常连接（DHCP） | 开启 `ESP-TEST` 热点后上电 | 串口打印 DHCP 分配的 IP，浏览器访问该 IP 出现控制台页 |
| 2 | AP 兜底切换 | 关闭 `ESP-TEST` 后上电 | 串口每 10s 报剩余时间，2 分钟后切换 AP；手机连 `ESP32-Direct`，访问 `192.168.4.1` 出现橙色「AP 直连模式」页面 |
| 3 | 掉线恢复 | STA 模式下临时关闭路由器 | 串口出现 `[离线]`，路由器恢复后自动重连（无需重启） |
| 4 | 从机上线 | 网络调试助手 TCP Client 连接 `主机IP:8000`，发送 `@F8266-1 online/` | 端口 8000 卡片绿点 + 「F8266-1 已上线」，日志出现 ★ 记录 |
| 5 | 主从互发 | 上例连接中，网页端口卡输入内容点「发送」；助手再发任意行 | 双向报文均出现在日志（← 收 / → 发），收发计数递增 |
| 6 | 从机离线（正常断开） | 断开调试助手连接 | 端口卡立即变「无从机」，日志出现 ○ 离线记录 |
| 7 | 从机离线（异常掉电） | 真实 ESP 从机连 8000 后直接拔电（调试助手断开会发 FIN，无法模拟） | 约 5-10s 内端口卡自动变灰，串口出现离线记录（TCP keepalive 感知） |
| 8 | 串口无输出 | — | 检查串口波特率为 115200；本板使用 USB CDC，换 USB 线/口重试 |

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
- **开启了固定 IP 后 STA 反而连不上**：`STA_USE_STATIC_IP=1` 时，固定 IP 必须与路由器同网段、网关须为路由器 IP；手机热点网段常见为安卓 `192.168.43.x`、iPhone `172.20.10.x`，请按实际网段修改 `config.h` 中的 `STA_STATIC_IP/STA_GATEWAY/STA_DNS`；一般环境建议保持默认 DHCP（`0`）
- **DHCP 下如何得知主机 IP**：上电后串口会打印实际分配的 IP，网页 `/api/status` 也返回；从机需先获得主机 IP 再连接
- **`esp32s3.local` 打不开**：mDNS 依赖系统/浏览器支持，Windows 10+、iOS、macOS 一般可用，Android 常不支持，此时直接用 IP 访问
- **AP 模式下手机显示「无法访问互联网」**：正常现象（设备热点本来就不提供外网），选择仍然连接即可
- **板载 RGB 灯为什么只有一个引脚（GPIO48）**：该板载灯是 **NeoPixel/WS2812 智能灯**，单数据线串行协议（每颗灯珠内置驱动芯片，一根数据线即可控制 RGB 三色），并非传统需要 3 路 PWM 的共阴/共阳 RGB。上电拉低 `RGB_LED_PIN`（GPIO48）即可保持熄灭；如需点亮，配合 `Adafruit_NeoPixel` 库（`NEO_GRB + NEO_KHZ800`）向该引脚发数据即可
- **WiFiManager 库去哪了**：AP 兜底 + 自有状态页已覆盖其门户功能，为保持精简已移除依赖；如需「网页改配 WiFi」可重新引入做按需门户

## 13. 维护约定

> **改动代码时同步更新本文档对应章节**，保持文档与代码一致：

- 改参数/默认值 → 更新 §6 配置表
- 改模式切换逻辑/超时行为 → 更新 §7 运行逻辑
- 增删路由或 JSON 字段 → 更新 §9 接口文档
- 新增文件/模块 → 更新 §4 目录结构
- 每次同步更新文档版本日期
