# F8266-1：ESP8266（Adafruit HUZZAH）WiFi 联网模块项目

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-05

## 项目概述

基于 PlatformIO + Arduino 的 ESP8266（Adafruit HUZZAH ESP8266，4MB Flash）测试项目。当前固件以 **WiFiManager 驱动 STA 模式**联网：

- 开机直连硬编码默认网络 `ESP-TEST` / `88888888`；
- 默认网络不可达时，自动打开**同名配置热点** `F8266-1`（192.168.4.1），供手机/电脑改配无线参数；
- 设备 STA 主机名固定为 `F8266-1`，在路由器客户端列表 / 局域网内以此名出现；
- 联网后作为 TCP 客户端连主机 [esp32-1](../esp32-1/)（端口 8000），连上即上报上线，并收发 `@命令/` 帧；
- **主机动态发现**：主机的 IP 可能由 DHCP 动态分配，从机监听主机在局域网 UDP 端口 `45555` 的广播宣告（`ESP32HOST,<ip>,...`）自动获知主机当前 IP，无需写死；主机 IP 变化会自动跟随，超过 20s 未收到宣告则回退到 `net_config.h` 的硬编码地址。

**业务定位（智慧农场）**：本机规划为智慧农场子系统的**状态节点**——逐步接入各农业子系统（传感器采集 / 执行控制等），本机 OLED 作为**状态可视化层**，最终汇总展示各子系统是否工作正常。当前完成该层的第一步：OLED 激活点亮。

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

## 当前功能

代码按职责拆分到独立模块，`main.cpp` 仅做编排（详见"目录结构"）：

1. **默认网络直连（STA）**：开机以 STA 模式直连 `ESP-TEST`（密码 `88888888`），最多等待 12s；成功后打印 IP 并进入主循环。
2. **同名配置热点兜底**：默认网络连不上时自动开启配置热点 `F8266-1`（WiFiManager），手机连接后访问 <http://192.168.4.1> 可改配其他无线网络；热点最长停留 180s，超时无人配置则自动重启重试。
3. **断线自愈**：运行期失联超过 15s（SDK 自动重连未恢复）自动重连默认网络 / 重新开配置热点。
4. **串口日志**：串口打印联网全过程（SSID / IP / hostname / 各阶段状态），便于排障。
5. **主机 TCP 链路**：联网后自动作为 TCP 客户端连主机 `:8000`，连上即发上线帧 `@F8266-1 online/`（主机网页对应端口变绿）；在线期间接收主机 `@…/` 命令帧并回调 `onHostCommand()`（命令内容留扩展口，当前仅串口打印）；主机离线自动按重试间隔重连，重连成功重新上报上线；WiFi 掉线期间链路自动暂停，恢复后重连。
6. **主机动态发现**：`host_link` 监听主机 UDP 广播（端口 `45555`），收到 `ESP32HOST,<ip>,...` 宣告即把目标主机切到该 IP（连旧地址则断开重连）；超过 `NET_HOST_DISCOVER_STALE_MS`（20s）无宣告则回退 `NET_HOST_IP`。主机为 DHCP 动态 IP 时从机仍能自动找到它。
7. **OLED 激活（业务层第一步）**：0.96" SSD1306（I2C：SDA=GPIO4、SCL=GPIO5，地址 0x3C）开机即点亮，首屏显示设备名 / 项目名 / `OLED OK`，作为智慧农场**状态可视化层**入口；后续各子系统健康与状态由本屏汇总展示（文字统一 ASCII/英文，暂不引入中文字库）。

> 说明：凭据为硬编码默认值，重启后始终优先尝试 `ESP-TEST`；配置热点仅作断网/改网兜底入口。无线参数与主机连接参数集中在 [src/net_config.h](src/net_config.h) 一处修改。

## 目录结构

```text
F8266-1/
├── src/
│   ├── main.cpp        # 入口：setup 联网并初始化主机链路，loop 调 net/link 的 handle + 命令回调
│   ├── net_config.h    # 网络与主机参数集中处（SSID/密码/名称/超时/主机 IP/端口，改配置只动此文件）
│   ├── wifi_net.h      # WifiNet 模块对外接口（begin / handle / isConnected）
│   ├── wifi_net.cpp    # WifiNet 实现：封装 WiFiManager，细节全部收敛在模块内
│   ├── host_link.h     # HostLink 模块对外接口（begin / handle / isOnline / send / onCommand）
│   ├── host_link.cpp   # HostLink 实现：TCP 连主机、UDP 广播发现主机、上线帧、@…/ 帧收发与断线重连（pimpl）
│   ├── oled_ctrl.h     # OledCtrl 模块对外接口（begin / isOk / handle）
│   └── oled_ctrl.cpp   # OledCtrl 实现：SSD1306 I2C 初始化与显示渲染（激活首屏，pimpl）
├── include/            # 项目头文件（PlatformIO 标准目录，当前为空）
├── lib/                # 项目私有库（PlatformIO 标准目录，当前为空）
├── test/               # 单元测试（PlatformIO 标准目录，当前为空）
├── platformio.ini      # PlatformIO 配置（依赖 tzapu/WiFiManager + Adafruit SSD1306/GFX）
└── README.md           # 本文档（仓库根另有 README 索引各板卡项目）
```

## 使用方法

```bash
pio run -d F8266-1              # 编译（在仓库根执行）
pio run -d F8266-1 -t upload    # 烧录到开发板
pio device monitor              # 打开串口监视器（115200）
```

VSCode 中直接打开 `F8266-1` 文件夹即可用 PlatformIO IDE 的构建/烧录按钮。

**预期现象**：烧录上电后，若环境存在 `ESP-TEST` 网络，串口打印连上后的 IP；若不存在，设备变为配置热点 `F8266-1`，手机连上后浏览器访问 <http://192.168.4.1> 可配置网络。连上默认网络后，只要主机 esp32-1 在线（DHCP 动态 IP 亦可，从机会收到其广播宣告自动切换目标地址；串口会打印「广播发现主机: <IP>」），串口即打印「上报上线」，主机网页 8000 端口卡变绿显示「F8266-1 已上线」（主机与从机须在同一局域网网段，且 `NET_HOST_ANNOUNCE_PORT` 与主机 `HOST_ANNOUNCE_PORT` 一致）。

## 模块扩展指引

后期新增功能（如 LED/继电器控制、由主机命令驱动的具体业务）时，请遵循**解耦约定**：

1. 每个功能一个模块文件（如 `led_ctrl.h/.cpp`），只暴露少量接口方法；
2. 在 `main.cpp` 顶部实例化，并在 `loop()` 的"业务模块扩展位"调用其 handle；
3. 模块内第三方库 include 只写在 `.cpp`，头文件不泄漏实现细节；
4. 网络 / 主机参数变化只改 `net_config.h`，业务代码不感知。

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
| 2026-09-05 | 业务层起步：新增 `oled_ctrl` 模块，激活 0.96" SSD1306 OLED（I2C GPIO4/5，地址 0x3C），开机点亮显示设备名 / SmartFarm / OLED OK，作为智慧农场状态可视化层入口；依赖新增 Adafruit SSD1306/GFX，编译通过 |
| 2026-09-05 | 主机 IP 支持 **UDP 广播动态发现**：`host_link` 监听主机宣告（端口 45555）自动采用发现地址，超时回退硬编码 IP；`net_config.h` 增发现参数，README 同步，编译通过 |
| 2026-09-05 | 构建板型由 ESP-01 换为 **Adafruit HUZZAH ESP8266**（`board=huzzah`，4MB Flash），说明文档同步 |
| 2026-09-05 | 接入主机通信层：新增 `host_link` 模块，联网后作为 TCP 客户端连主机 `192.168.1.100:8000`，连上上报 `@F8266-1 online/`，`@…/` 帧收发 + 断线自动重连；命令内容留 `onHostCommand()` 扩展口；主机连接参数进 `net_config.h`，编译通过 |
| 2026-09-04 | 创建项目：WiFiManager STA 联网模块化实现（`net_config.h` + `wifi_net` 模块 + 精简 `main.cpp`），直连 `ESP-TEST/88888888`，同名配置热点 `F8266-1` 兜底 + 断线自愈；加入仓库 sandbox-solution 顶层目录，编译通过 |
