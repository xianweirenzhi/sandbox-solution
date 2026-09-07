# F8266-1：ESP8266（Adafruit HUZZAH）WiFi 联网模块项目

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-06

## 项目概述

基于 PlatformIO + Arduino 的 ESP8266（Adafruit HUZZAH ESP8266，4MB Flash）测试项目。当前固件以 **WiFiManager 驱动 STA 模式**联网：

- 开机直连硬编码默认网络 `ESP-TEST` / `88888888`；
- 默认网络不可达时，自动打开**同名配置热点** `F8266-1`（192.168.4.1），供手机/电脑改配无线参数；
- 设备 STA 主机名固定为 `F8266-1`，在路由器客户端列表 / 局域网内以此名出现；
- 联网后作为 TCP 客户端连主机 [esp32-1](../esp32-1/)（端口 8000），连上即上报上线，并收发 `@命令/` 帧；
- **主机动态发现**：主机的 IP 可能由 DHCP 动态分配，从机监听主机在局域网 UDP 端口 `45555` 的广播宣告（`ESP32HOST,<ip>,...`）自动获知主机当前 IP，无需写死；主机 IP 变化会自动跟随，超过 20s 未收到宣告则回退到 `net_config.h` 的硬编码地址。

**业务定位（智慧农场）**：本机规划为智慧农场子系统的**状态节点**——逐步接入各农业子系统（传感器采集 / 执行控制等），本机 OLED 作为**状态可视化层**，汇总展示各子系统是否工作正常。当前进度：OLED 已激活（信息区 4 行接口就绪）；**子系统① SHT30 温湿度、子系统② GY-30 光照已接入**，读数/故障实时上屏（每子系统一行）。

**命名约定**：设备身份、STA 主机名、配置热点名统一使用 `F8266-1`，与项目目录同名。该身份对应仓库 [esp32-1](../esp32-1/) 主机 6 路 TCP 从机端口中的 `F8266-x` 命名体系——F8266-1 接入主机 **8000** 端口。主机 IP 默认支持**动态发现**（监听主机 UDP 广播）；回退地址 `192.168.1.100` 仅在网络中无主机宣告时兜底（改主机 IP / 端口 / 广播参数只动 [net_config.h](src/net_config.h)，须保证与从机同一网段）。命令详细内容尚未定义，已留 `onHostCommand()` 扩展口待后续业务迭代填充。

本项目位于仓库 [sandbox-solution](https://github.com/xianweirenzhi/sandbox-solution)（公开）的 `F8266-1/` 子目录——仓库为多板卡项目集合，其他板卡项目以同级目录存放，详见仓库根 [README](../README.md)。

## 硬件信息

| 项 | 值 |
| --- | --- |
| 开发板 | Adafruit HUZZAH ESP8266（ESP8266EX，4MB Flash） |
| 板型（board） | `huzzah` |
| 平台 | espressif8266（PlatformIO） |
| 框架 | Arduino |
| 环境名（env） | `esp01_1m`（沿用早期命名，仅作构建标签） |
| 串口波特率 | 115200 |

**当前外设接线**（智慧农场逐步接入）：

| 外设 | 接口 | 接线 |
| --- | --- | --- |
| OLED 0.96"（SSD1306） | I2C | SDA→板上 SDA（GPIO4）、SCL→板上 SCL（GPIO5）、VCC→3V3、GND→GND；I2C 地址 0x3C |
| SHT30 温湿度传感器 | I2C（与 OLED 共线） | SDA/SCL 并接到同一总线、VCC→3V3、GND→GND；I2C 地址 0x44（ADDR 接高则 0x45） |
| GY-30 光照传感器（BH1750） | I2C（与 OLED 共线） | SDA/SCL 并接到同一总线、VCC→3V3、GND→GND；I2C 地址 0x23（ADDR 接高则 0x5C） |

## 当前功能

代码按职责拆分到独立模块，`main.cpp` 仅做编排（详见"目录结构"）：

1. **默认网络直连（STA）**：开机以 STA 模式直连 `ESP-TEST`（密码 `88888888`），最多等待 12s；成功后打印 IP 并进入主循环。
2. **同名配置热点兜底**：默认网络连不上时自动开启配置热点 `F8266-1`（WiFiManager），手机连接后访问 <http://192.168.4.1> 可改配其他无线网络；热点最长停留 180s，超时无人配置则自动重启重试。
3. **断线自愈**：运行期失联超过 15s（SDK 自动重连未恢复）自动重连默认网络 / 重新开配置热点。
4. **串口日志**：串口打印联网全过程（SSID / IP / hostname / 各阶段状态），便于排障。
5. **主机 TCP 链路**：联网后自动作为 TCP 客户端连主机 `:8000`，连上即发上线帧 `@F8266-1 online/`（主机网页对应端口变绿）；在线期间接收主机 `@…/` 命令帧并回调 `onHostCommand()`（命令内容留扩展口，当前仅串口打印）；主机离线自动按重试间隔重连，重连成功重新上报上线；WiFi 掉线期间链路自动暂停，恢复后重连。
6. **主机动态发现**：`host_link` 监听主机 UDP 广播（端口 `45555`），收到 `ESP32HOST,<ip>,...` 宣告即把目标主机切到该 IP（连旧地址则断开重连）；超过 `NET_HOST_DISCOVER_STALE_MS`（20s）无宣告则回退 `NET_HOST_IP`。主机为 DHCP 动态 IP 时从机仍能自动找到它。
7. **OLED 状态屏（状态可视化层）**：0.96" SSD1306（I2C：SDA=GPIO4、SCL=GPIO5，地址 0x3C）开机即点亮，标题区显示设备名 / 项目名 / `OLED OK`；下方**信息区 4 行**由各子系统经 `setInfoLine(row, ...)` 写入状态文本（每子系统一行，通用接口，行内容变化才重绘）（文字统一 ASCII/英文，暂不引入中文字库）。
8. **SHT30 温湿度采集（子系统①）**：I2C 地址 0x44，每 2s 采集一次温/湿度（读数带 CRC 校验）；**健康判定**——连续 3 次读取失败标记故障（OLED 行 0 显示 `SHT30 ERR`），恢复自动转好；上电未检测到传感器不阻塞启动，每 10s 重扫（支持热插拔自愈）；读数/故障每秒刷新到 OLED（如 `T:25.3C H:56%`），串口同步打印。
9. **GY-30 光照采集（子系统②）**：板载 BH1750，I2C 地址 0x23（ADDR 接高 0x5C），连续高分辨率模式（1 lx 分辨率），每 2s 采集一次；健康判定 / 热插拔重扫机制同子系统①（故障显示 `GY30 ERR`，行 1）；读数如 `L:1234lux` 上屏，串口同步打印。

> 说明：凭据为硬编码默认值，重启后始终优先尝试 `ESP-TEST`；配置热点仅作断网/改网兜底入口。无线参数与主机连接参数集中在 [src/net_config.h](src/net_config.h) 一处修改。

## 目录结构

```text
F8266-1/
├── src/
│   ├── main.cpp        # 入口：setup 激活 OLED/传感器 + 联网 + 主机链路，loop 调各模块 handle + 状态上屏
│   ├── net_config.h    # 网络与主机参数 + 外设引脚集中处（SSID/密码/名称/超时/主机 IP/端口/I2C 引脚，改配置只动此文件）
│   ├── wifi_net.h      # WifiNet 模块对外接口（begin / handle / isConnected）
│   ├── wifi_net.cpp    # WifiNet 实现：封装 WiFiManager，细节全部收敛在模块内
│   ├── host_link.h     # HostLink 模块对外接口（begin / handle / isOnline / send / onCommand）
│   ├── host_link.cpp   # HostLink 实现：TCP 连主机、UDP 广播发现主机、上线帧、@…/ 帧收发与断线重连（pimpl）
│   ├── oled_ctrl.h     # OledCtrl 模块对外接口（begin / isOk / setInfoLine / handle）
│   ├── oled_ctrl.cpp   # OledCtrl 实现：SSD1306 I2C 初始化、标题区 + 信息行渲染（pimpl）
│   ├── sht30_sensor.h  # Sht30Sensor 模块对外接口（begin / handle / isOk / hasData / getTempC / getHumRH）
│   ├── sht30_sensor.cpp # Sht30Sensor 实现：周期采集、健康判定、热插拔重扫（pimpl）
│   ├── bh1750_sensor.h  # Bh1750Sensor 模块对外接口（begin / handle / isOk / hasData / getLux）
│   └── bh1750_sensor.cpp # Bh1750Sensor 实现：光照周期采集、健康判定、热插拔重扫（pimpl，同构 SHT30）
├── include/            # 项目头文件（PlatformIO 标准目录，当前为空）
├── lib/                # 项目私有库（PlatformIO 标准目录，当前为空）
├── test/               # 单元测试（PlatformIO 标准目录，当前为空）
├── platformio.ini      # PlatformIO 配置（依赖 tzapu/WiFiManager + Adafruit SSD1306/GFX/SHT31 + claws/BH1750）
└── README.md           # 本文档（仓库根另有 README 索引各板卡项目）
```

## 使用方法

```bash
pio run -d F8266-1              # 编译（在仓库根执行）
pio run -d F8266-1 -t upload    # 烧录到开发板
pio device monitor              # 打开串口监视器（115200）
```

VSCode 中直接打开 `F8266-1` 文件夹即可用 PlatformIO IDE 的构建/烧录按钮。

**预期现象**：烧录上电后，OLED 即点亮首屏（`F8266-1` / `SmartFarm` / `OLED OK`），串口打印 `[oled] SSD1306 已激活`——若屏不亮，检查接线并把 `oled_ctrl.cpp` 的 `OLED_I2C_ADDR` 在 0x3C/0x3D 间切换（OLED 失败不影响后续联网）。SHT30 接好时串口打印 `[sht] SHT30 已检测到（0x44）`，数秒后 OLED 信息区行 0 显示 `T:xx.xC H:xx%`（每 2s 更新读数、串口同步打印）；未接/故障时该行显示 `SHT30 ERR`（每 10s 自动重扫，接上即恢复）。GY-30 接好时串口打印 `[gy30] GY-30(BH1750) 已检测到（0x23）`，行 1 显示 `L:xxxxlux`（手机手电筒照射可见读数明显变化）；未接/故障时显示 `GY30 ERR`（同上自愈）。随后若环境存在 `ESP-TEST` 网络，串口打印连上后的 IP；若不存在，设备变为配置热点 `F8266-1`，手机连上后浏览器访问 <http://192.168.4.1> 可配置网络。连上默认网络后，只要主机 esp32-1 在线（DHCP 动态 IP 亦可，从机会收到其广播宣告自动切换目标地址；串口会打印「广播发现主机: `<IP>`」），串口即打印「上报上线」，主机网页 8000 端口卡变绿显示「F8266-1 已上线」（主机与从机须在同一局域网网段，且 `NET_HOST_ANNOUNCE_PORT` 与主机 `HOST_ANNOUNCE_PORT` 一致）。

## 模块扩展指引

后期新增功能（如 LED/继电器控制、由主机命令驱动的具体业务）时，请遵循**解耦约定**：

1. 每个功能一个模块文件（如 `led_ctrl.h/.cpp`），只暴露少量接口方法；
2. 在 `main.cpp` 顶部实例化，并在 `loop()` 的"业务模块扩展位"调用其 handle；
3. 模块内第三方库 include 只写在 `.cpp`，头文件不泄漏实现细节；
4. 网络 / 主机参数 / 外设引脚变化只改 `net_config.h`，业务代码不感知。

与主机通信的约定：主机下发命令的内容解析与执行，在 `main.cpp` 的 `onHostCommand()` 回调中扩展；从机需要主动上报/回包时，调用 `link.send("内容")`（`host_link` 自动包成 `@内容/` 帧，已自动剥离帧头尾；帧内容勿含 `@` / `/`，单帧内容上限见 `net_config.h` 的 `NET_FRAME_MAX_LEN`，默认 64 字符）。

## 维护约定（重要）

本仓库采用 **“代码改动 ⇄ 文档 + git 同步更新”** 制，每次变更必须一起完成并推送：

1. 修改代码（本目录内）后，**同步更新本文档**：功能说明有变化就改写，无论大小都在"更新记录"补一行（按日期）；
2. 若项目概览（目录、依赖、命名等）有变，同步更新本 README 对应小节；
3. 涉及仓库级变更（新增/移除板卡、构建方式调整）时，同步更新仓库根 [README](../README.md) 的索引表与更新记录；
4. 代码与文档放在同一次 git 提交（或紧邻的两次提交）中推送，保证远端文档始终与代码一致。

## 更新记录

| 日期 | 变更内容 |
| --- | --- |
| 2026-09-06 | 子系统②接入：新增 `bh1750_sensor` 模块（GY-30/BH1750 光照采集，I2C 0x23 共线，连续高分辨率模式，同构子系统①的健康判定/热插拔重扫）；`oled_ctrl` 信息行升级为**信息区 4 行**（`setInfoLine(row, ...)`，每子系统一行）；依赖新增 claws/BH1750，编译通过 |
| 2026-09-06 | 子系统①接入：新增 `sht30_sensor` 模块（SHT30 温湿度采集，I2C 0x44 与 OLED 共线，每 2s 采集 + CRC 校验 + 连续失败判故障 + 10s 热插拔重扫）；`oled_ctrl` 增通用信息行接口 `setInfoLine`，温湿度/故障实时上屏；I2C 引脚集中入 `net_config.h`；依赖新增 Adafruit SHT31，编译通过 |
| 2026-09-05 | 业务层起步：新增 `oled_ctrl` 模块，激活 0.96" SSD1306 OLED（I2C GPIO4/5，地址 0x3C），开机点亮显示设备名 / SmartFarm / OLED OK，作为智慧农场状态可视化层入口；依赖新增 Adafruit SSD1306/GFX，编译通过 |
| 2026-09-05 | 主机 IP 支持 **UDP 广播动态发现**：`host_link` 监听主机宣告（端口 45555）自动采用发现地址，超时回退硬编码 IP；`net_config.h` 增发现参数，README 同步，编译通过 |
| 2026-09-05 | 构建板型由 ESP-01 换为 **Adafruit HUZZAH ESP8266**（`board=huzzah`，4MB Flash），说明文档同步 |
| 2026-09-05 | 接入主机通信层：新增 `host_link` 模块，联网后作为 TCP 客户端连主机 `192.168.1.100:8000`，连上上报 `@F8266-1 online/`，`@…/` 帧收发 + 断线自动重连；命令内容留 `onHostCommand()` 扩展口；主机连接参数进 `net_config.h`，编译通过 |
| 2026-09-04 | 创建项目：WiFiManager STA 联网模块化实现（`net_config.h` + `wifi_net` 模块 + 精简 `main.cpp`），直连 `ESP-TEST/88888888`，同名配置热点 `F8266-1` 兜底 + 断线自愈；加入仓库 sandbox-solution 顶层目录，编译通过 |
