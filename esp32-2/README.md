# ESP32-S3 智慧农场控制中心（六从机可视化主机 · esp32-1 网页升级版）

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-09

## 1. 项目简介

本项目是 [esp32-1](../esp32-1/) 的**完整副本差异化**产物：通信/采集/宣告等主机固件逻辑与 esp32-1 一致（STA 优先 + 超时 AP 兜底、6 个连续 TCP 从机端口 8000~8005、UDP 广播宣告、TCP keepalive 掉电感知），在此基础做了两项核心升级：

1. **六从机全量可视化 + 每从机独立自动控制（硬件逻辑升级）**：原版仅 F8266-1（端口 8000）有传感图形化与阈值自动控制；本版将 `auto_ctrl` 泛化为**每个端口一套独立实例**（各存 NVS 互不影响），网页端六个从机的传感数据全部图形化呈现，且每从机均有「手动 / 自动」两种模式；
2. **网页 v2 响应式仪表盘（界面升级）**：参照成熟 SaaS 仪表盘风格（卡片式布局、语义色、圆角浅阴影、深色模式跟随系统）重写，自适应**手机浏览器（竖屏单列）与 PC 浏览器（宽屏多列）**两种形态。

硬件：4D Systems gen4-ESP32S3 (R8N16)，框架：Arduino (PlatformIO)。位于仓库 [sandbox-solution](https://github.com/xianweirenzhi/sandbox-solution) 的 `esp32-2/` 子目录，详见仓库根 [README](../README.md)。

## 2. 功能特性

### 与 esp32-1 一致的主机能力

- [x] STA 模式直连路由器（DHCP/固定 IP 可配），2 分钟超时自动切 AP 兜底（热点 `ESP32-Direct-2`）
- [x] 6 个连续 TCP 端口（8000~8005），`@命令/` 帧式收发、`@F8266-x online/` 上线识别、断开/掉电自动离线
- [x] 主机 UDP 广播宣告（从机动态发现主机 IP）
- [x] 传感 JSON 帧（t/h/lux/soil/co2/tvoc/pump/fan/servo）解析存储，`/api/ports` 全量暴露

### 本版升级点

- [x] **从机总览**：六从机卡片同屏（在线状态/端口/收发计数），每卡 6 指标迷你进度条（温度/湿度/光照/土壤/CO₂/TVOC）+ 执行器状态章（泵/扇/棚），点击卡片直达该从机详情
- [x] **从机详情**：传感大卡（大数值 + 进度条 + **近 60 点迷你趋势折线**，canvas 绘制）+ 执行器控制（水泵开关/风扇正反转停/舵机滑条，命令下发到该从机端口）+ **该从机独立的自动控制卡**（手动/自动切换、3 阈值编辑保存、4 规则触发状态）
- [x] **每端口独立自动控制**：`auto_ctrl` 由单实例改为 `PORT_COUNT` 实例数组，规则不变（高温/高CO₂→风扇正转、高湿→反转排湿、超阈 120% 开大棚、土壤干→水泵，滞回 + 连续 3 次确认 + 每周期按回报校正）；配置按端口序号存 NVS（键 `en0/tHi0/...~en5/tHi5`），互不干扰
- [x] **响应式布局**：CSS Grid `auto-fill/minmax` 自适应——手机竖屏单列/两列，平板两列，PC 宽屏三列以上（最大 1160px）；顶栏吸顶
- [x] **深色模式**：跟随系统 `prefers-color-scheme`，无需手动切换
- [x] `/api/auto` 升级：GET/POST 均支持 `port` 参数（缺省 8000，兼容原用法）

### 身份与 esp32-1 的差异（同网共存不冲突）

| 项 | esp32-1 | esp32-2 |
|---|---|---|
| mDNS 主机名 | `esp32s3.local` | `esp32s3-2.local` |
| AP 兜底热点 | `ESP32-Direct` | `ESP32-Direct-2` |
| 构建环境名 | 默认（板型名） | `esp32_2` |
| TCP 从机端口 | 8000~8005 | 8000~8005（同段部署时仅接一台主机即可） |

## 3. 开发环境

| 项 | 值 |
|---|---|
| IDE | VSCode + PlatformIO |
| 平台 | espressif32 |
| 开发板 | `4d_systems_esp32s3_gen4_r8n16`（ESP32-S3, 16MB Flash / 8MB PSRAM） |
| 框架 | Arduino |
| 串口 | 115200，USB CDC |
| 外部库 | bblanchon/ArduinoJson（帧解析） |

## 4. 目录结构

```
esp32-2/
├── platformio.ini        PlatformIO 工程配置（环境名 esp32_2）
├── README.md             本文档
└── src/
    ├── main.cpp          入口：流程编排 + 串口连接信息打印 + 心跳
    ├── config.h          ★ 全部可调参数（WiFi、AP 兜底、端口、广播宣告、自动控制阈值默认值）
    ├── wifi_service.h/.cpp   WiFi 管理：STA 优先(DHCP/固定 IP) → 超时切 AP
    ├── device_status.h/.cpp 状态采集：collectStatus() 返回 DeviceStatus 结构体
    ├── port_service.h/.cpp  从机通信：6 个 TCP 端口收发、上线识别、传感 JSON 解析、keepalive 掉电探测、日志环形缓冲
    ├── host_announce.h/.cpp 主机 UDP 广播宣告
    ├── auto_ctrl.h/.cpp     ★ 智能大棚自动控制：**每端口独立实例**，4 规则滞回防抖 + 按回报校正，配置按端口存 NVS
    └── web_ui.h/.cpp     ★ 网页服务 v2：响应式仪表盘（总览/详情/日志三级视图）、路由注册、JSON 接口
```

**分层依赖**：`main` → `wifi_service` / `port_service` / `web_ui` / `device_status`；`web_ui` → `port_service` + `auto_ctrl`；`auto_ctrl` → `port_service`（读快照/发命令）；参数统一来自 `config.h`。

## 5. 快速开始

1. 用 VSCode 打开项目文件夹，PlatformIO 自动识别 `platformio.ini`
2. 修改 `src/config.h` 中 `STA_SSID` / `STA_PASS` 为实际路由器/热点凭据
3. **Upload** 编译烧录，Serial Monitor（115200）观察连接
4. 按串口提示的 URL 访问（或 `http://esp32s3-2.local/`）：

```
[Web] 网页服务已启动: http://192.168.x.x/
[Web] 部分设备也可用: http://esp32s3-2.local/
```

命令行：

```bash
pio run -d esp32-2                # 编译
pio run -d esp32-2 -t upload      # 烧录
pio device monitor                # 串口监视
```

## 6. 配置说明（config.h）

与 esp32-1 完全一致（含 STA/AP、端口、广播宣告、`AC_*` 自动控制默认阈值），仅以下身份项不同，其余见 [esp32-1 README §6](../esp32-1/README.md)：

| 宏 | 默认值 | 说明 |
|---|---|---|
| `FALLBACK_AP_SSID` | `"ESP32-Direct-2"` | 兜底热点名（与 esp32-1 区分） |
| `MDNS_HOST` | `"esp32s3-2"` | mDNS 主机名（`esp32s3-2.local`） |

## 7. 网页使用说明（v2）

```
┌────────────────────────────────────────────┐
│ ● 智慧农场控制中心  esp32s3-2·IP   [运行时间] [📋日志] │ ← 吸顶顶栏
├────────────────────────────────────────────┤
│ 从机总览  在线 n/6                           │
│ ┌─────────┐ ┌─────────┐ ┌─────────┐        │ ← 六从机卡片
│ │●F8266-1 │ │●F8266-2 │ │●F8266-3 │ …      │   (手机单列/PC 多列)
│ │ 6 指标   │ │         │ │         │        │
│ │ 泵 扇 棚 │ │         │ │         │        │
│ └─────────┘ └─────────┘ └─────────┘        │
│ 从机详情            [●F8266-1][●F8266-2]…  │ ← 标签切换从机
│ ┌ 传感大卡×6(数值+进度条+趋势线) ┐            │
│ ┌ 执行器控制(按选中从机下发) ┐┌ 自动控制(独立阈值) ┐ │
└────────────────────────────────────────────┘
```

- **总览卡**点击 → 平滑滚动到该从机详情；详情标签可随时切换从机
- **趋势折线**：详情页每指标保留近 60 个采样（约 1.5 分钟），切换从机后重新累积
- **手动模式**：执行器按键/滑条直接下发到选中从机端口
- **自动模式**：每从机独立开关与阈值（存 NVS，重启保留）；规则触发状态红点实时显示
- **通信日志**：二级页六端口收发记录 + 每端口手动发送框（PC 三列/手机两列）

## 8. Web 接口文档

| 路由 | 方法 | 说明 |
|---|---|---|
| `/` | GET | 控制中心页面 v2（响应式仪表盘，存于 Flash/PROGMEM） |
| `/api/status` | GET | 设备状态 JSON（同 esp32-1） |
| `/api/ports` | GET | 6 端口状态 + 最新传感数据 JSON 数组（同 esp32-1） |
| `/api/send` | POST | 向指定端口发报文/命令，参数 `port`、`text`（同 esp32-1） |
| `/api/auto` | GET | **升级**：参数 `port`（8000~8005，缺省 8000），返回该端口配置 `enabled/tHigh/co2High/hHigh`、期望状态 `fanDir/pumpActive/servoActive` 与 4 条规则触发状态 |
| `/api/auto` | POST | **升级**：参数 `port` + `enabled/tHigh/co2High/hHigh`，写入该端口 NVS |
| 其他 | — | 404 |

`/api/ports`、`/api/status` 字段说明与从机通信帧协议（`@命令/`、上线帧、2s JSON 上报、执行器命令词表 PUMP/FAN/SERVO、TCP keepalive）均与 esp32-1 一致，见 [esp32-1 README §9/§9.5](../esp32-1/README.md)。

## 9. 与 esp32-1 的代码差异一览

| 文件 | 差异 |
|---|---|
| `src/web_ui.cpp` | INDEX_HTML 全量重写（响应式仪表盘 + canvas 趋势线 + 每从机控制）；`handleGetAuto/handleSetAuto` 增加 `port` 参数（`autoIdxFromArg()` 解析与校验） |
| `src/auto_ctrl.h/.cpp` | 单实例 → `PortAuto s_pa[PORT_COUNT]` 每端口实例；`getConfig/setConfig/fanDir/pumpActive/servoActive/ruleState` 增加 `idx` 参数；NVS 键名按端口序号后缀化 |
| `src/config.h` | `FALLBACK_AP_SSID`、`MDNS_HOST` 身份差异化 |
| `platformio.ini` | 环境名 `[env:esp32_2]` |
| 其余文件 | 与 esp32-1 相同 |

## 10. 维护约定

> **改动代码时同步更新本文档对应章节**，保持文档与代码一致；与 esp32-1 共性内容的文档（配置表、协议、FAQ）以 esp32-1 README 为准，本 README 只维护差异项，避免两处失同步。
