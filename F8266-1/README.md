# F8266-1：ESP8266（Adafruit HUZZAH）WiFi 联网模块项目

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-08

## 项目概述

基于 PlatformIO + Arduino 的 ESP8266（Adafruit HUZZAH ESP8266，4MB Flash）测试项目。当前固件以 **WiFiManager 驱动 STA 模式**联网：

- 开机直连硬编码默认网络 `ESP-TEST` / `88888888`；
- 默认网络不可达时，自动打开**同名配置热点** `F8266-1`（192.168.4.1），供手机/电脑改配无线参数；
- 设备 STA 主机名固定为 `F8266-1`，在路由器客户端列表 / 局域网内以此名出现；
- 联网后作为 TCP 客户端连主机 [esp32-1](../esp32-1/)（端口 8000），连上即上报上线，并收发 `@命令/` 帧；
- **主机动态发现**：主机的 IP 可能由 DHCP 动态分配，从机监听主机在局域网 UDP 端口 `45555` 的广播宣告（`ESP32HOST,<ip>,...`）自动获知主机当前 IP，无需写死；主机 IP 变化会自动跟随，超过 20s 未收到宣告则回退到 `net_config.h` 的硬编码地址。

**业务定位（智慧农场）**：本机规划为智慧农场子系统的**状态节点**——逐步接入各农业子系统（传感器采集 / 执行控制等），本机 OLED 作为**状态可视化层**，汇总展示各子系统是否工作正常。当前进度：**传感子系统① SHT30 温湿度、② GY-30 光照、③ MQ-2 可燃气体、④ 土壤湿度、⑤ SGP30 空气质量（eCO₂/TVOC）+ 首个执行实体（水泵/风扇/舵机）已接入**，读数/故障/执行器状态实时上屏（信息区 6 行满员）。

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
| MQ-2 可燃气体传感器 | ADC（A0） | **独立 5V 电源模块供电**（VCC/GND 接电源模块，**GND 必须与开发板共地**）；AO ──[5×1kΩ 串联]──┬──[1kΩ]──GND（5:1 分压），分压点┬接板上 ADC（HUZZAH ADC 量程 0~1V，分压后 5V→0.83V）；DO 悬空 |
| 土壤湿度传感器 | 数字 GPIO | VCC→开发板 3V3（保证 DO 高电平 3.3V）、GND→GND、**DO→GPIO14**（避开 boot 约束脚 0/2/15）；AO 悬空 |
| SGP30 空气质量（eCO₂/TVOC） | I2C（与 OLED 共线） | SDA/SCL 并接到同一总线、VIN→3V3、GND→GND；I2C 地址 0x58（固定，无地址选择脚） |
| 水泵（低触发继电器/MOS） | GPIO 输出 | 驱动模块 IN→**GPIO13**（低=开泵）；模块 VCC/GND 按模块接 3V3/GND；**泵体电源独立供电 + 共地** |
| 风扇（双路低触发，正反转） | GPIO 输出 ×2 | 驱动模块 IN1(正转)→**GPIO2**、IN2(反转)→**GPIO16**；**★两路严禁同时触发**（电源直通短路），固件已软件互锁；风扇电源独立 + 共地。注：GPIO2 为 boot 脚（上电需高），低触发设备上电默认关=安全；须确认风扇模块 IN 无下拉电阻（否则拉低 GPIO2 致无法启动） |
| 舵机（180° 摆角，SG90 类） | GPIO PWM | 信号线(橙)→**GPIO12**、VCC(红)→独立 5V、GND(褐)→共地 |

## 当前功能

代码按职责拆分到独立模块，`main.cpp` 仅做编排（详见"目录结构"）：

1. **默认网络直连（STA）**：开机以 STA 模式直连 `ESP-TEST`（密码 `88888888`），最多等待 12s；成功后打印 IP 并进入主循环。
2. **同名配置热点兜底**：默认网络连不上时自动开启配置热点 `F8266-1`（WiFiManager），手机连接后访问 <http://192.168.4.1> 可改配其他无线网络；热点最长停留 180s，超时无人配置则自动重启重试。
3. **断线自愈**：运行期失联超过 15s（SDK 自动重连未恢复）自动重连默认网络 / 重新开配置热点。
4. **串口日志**：串口打印联网全过程（SSID / IP / hostname / 各阶段状态），便于排障。
5. **主机 TCP 链路**：联网后自动作为 TCP 客户端连主机 `:8000`，连上即发上线帧 `@F8266-1 online/`（主机网页对应端口变绿）；在线期间接收主机 `@…/` 命令帧并回调 `onHostCommand()`（命令内容留扩展口，当前仅串口打印）；主机离线自动按重试间隔重连，重连成功重新上报上线；WiFi 掉线期间链路自动暂停，恢复后重连。
6. **主机动态发现**：`host_link` 监听主机 UDP 广播（端口 `45555`），收到 `ESP32HOST,<ip>,...` 宣告即把目标主机切到该 IP（连旧地址则断开重连）；超过 `NET_HOST_DISCOVER_STALE_MS`（20s）无宣告则回退 `NET_HOST_IP`。主机为 DHCP 动态 IP 时从机仍能自动找到它。
7. **OLED 状态屏（状态可视化层）**：0.96" SSD1306（I2C：SDA=GPIO4、SCL=GPIO5，地址 0x3C）开机即点亮，标题行（设备名 + SmartFarm 合并一行，激活验证期的 `OLED OK` 行已退役让位）；下方**信息区 6 行**由各子系统经 `setInfoLine(row, ...)` 写入状态文本（每子系统一行，通用接口，行内容变化才重绘）（文字统一 ASCII/英文，暂不引入中文字库）。
8. **SHT30 温湿度采集（子系统①）**：I2C 地址 0x44，每 2s 采集一次温/湿度（读数带 CRC 校验）；**健康判定**——连续 3 次读取失败标记故障（OLED 行 0 显示 `SHT30 ERR`），恢复自动转好；上电未检测到传感器不阻塞启动，每 10s 重扫（支持热插拔自愈）；读数/故障每秒刷新到 OLED（如 `T:25.3C H:56%`），串口同步打印。
9. **GY-30 光照采集（子系统②）**：板载 BH1750，I2C 地址 0x23（ADDR 接高 0x5C），连续高分辨率模式（1 lx 分辨率），每 2s 采集一次；健康判定 / 热插拔重扫机制同子系统①（故障显示 `GY30 ERR`，行 1）；读数如 `L:1234lux` 上屏，串口同步打印。
10. **MQ-2 可燃气体采集（子系统③）**：模拟量经 5×1kΩ/1kΩ 分压（5:1）接 ADC（A0，独立 5V 供电 + 共地；满量程 5V→0.83V≈850 counts），每 2s 采集（单周期 5 采样中值滤波，抑制 ESP8266 ADC 的 WiFi 干扰抖动）；**预热管理**——上电 3 分钟内为预热期（OLED 行 2 显示 `MQ2 HEAT`，读数不参与报警判定）；**阈值报警**——滤波值超过 `MQ2_ALARM_RAW`(300) 显示 `MQ2 GAS!`（清洁空气基线需现场标定后按需调整阈值）；**合理性监测**——预热后连续 1 分钟贴 0（疑似 AO 断线）或贴满量程（疑似分压失效）显示 `MQ2 ERR`，恢复自动转好；正常读数 `G:123` 上屏，串口同步打印。
11. **土壤湿度监测（子系统④）**：数字量 DO→GPIO14（模块 3V3 供电），判定语义按现场实测模块设定为 **高电平=湿度过低、低电平=湿润适宜**（`SOIL_HIGH_IS_MOIST=0`；换模块若极性相反改此宏即可）；**1s 环境消抖**——新电平须连续稳定 1s 才切换生效判定，临界湿度附近的高频跳变（浇水边缘/电极抖动）不会引起状态翻转；开机以当前电平为初始判定（不误报）；OLED 行 3 显示 `SOIL:WET` / `SOIL:DRY`，串口同步打印跳变与切换。
12. **SGP30 空气质量采集（子系统⑤）**：I2C 0x58 共线，每 2s 读 **eCO₂(ppm) + TVOC(ppb)**；★**eCO₂ 为 TVOC 推算的等效值**（非真实 CO₂ 浓度，酒精/VOC 会使其虚高，作空气新鲜度参考；后续从机需真 CO₂ 应选 SCD40 等），未做基线持久化（重启后重新积累，约 12h 达最优精度）；**15s 暖机**——上电/重扫后暖机期内读数不可靠且失败不判故障（OLED 显示 `SGP30 INIT`）；健康判定/热插拔重扫同子系统①②；OLED 行 4 显示 `C:800ppm V:12ppb` / `SGP30 ERR`，串口同步打印。
13. **执行实体（水泵/风扇/舵机，首个执行层）**：`actuator_ctrl` 模块，接口就绪、控制入口待接（主机命令/本地自动逻辑）；**上电安全态**——泵关（GPIO13 高）、风扇停（GPIO2/16 双高）、舵机回中位 90°（GPIO12，Servo 库 50Hz）；**风扇双路低触发正反转 + 软件互锁**（开任一路前强制断开另一路，杜绝两路同触导致驱动级电源直通短路——接线与后续命令必须遵守）；舵机 `servoSet(0~180°)` 自动夹取；OLED 行 5 显示 `P:OFF F:OFF S:90` 实时反映执行器状态，全部动作串口打印。

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
│   ├── bh1750_sensor.cpp # Bh1750Sensor 实现：光照周期采集、健康判定、热插拔重扫（pimpl，同构 SHT30）
│   ├── mq2_sensor.h    # Mq2Sensor 模块对外接口（begin / handle / isPreheat / isOk / isAlarm / getRaw）
│   ├── mq2_sensor.cpp  # Mq2Sensor 实现：ADC 中值滤波采集、预热管理、阈值报警、合理性监测（pimpl）
│   ├── soil_sensor.h   # SoilSensor 模块对外接口（begin / handle / isMoist / isDry / rawLevel）
│   ├── soil_sensor.cpp # SoilSensor 实现：DO 电平读取 + 1s 环境消抖状态机（pimpl）
│   ├── sgp30_sensor.h  # Sgp30Sensor 模块对外接口（begin / handle / isOk / isWarmup / hasData / getEco2Ppm / getTvocPpb）
│   ├── sgp30_sensor.cpp # Sgp30Sensor 实现：eCO₂/TVOC 周期采集、暖机、健康判定、热插拔重扫（pimpl）
│   ├── actuator_ctrl.h  # ActuatorCtrl 模块对外接口（泵开关 / 风扇正反转互锁 / 舵机角度）
│   └── actuator_ctrl.cpp # ActuatorCtrl 实现：低触发驱动 + 上电安全态 + 风扇软件互锁 + Servo（pimpl）
├── include/            # 项目头文件（PlatformIO 标准目录，当前为空）
├── lib/                # 项目私有库（PlatformIO 标准目录，当前为空）
├── test/               # 单元测试（PlatformIO 标准目录，当前为空）
├── platformio.ini      # PlatformIO 配置（依赖 tzapu/WiFiManager + Adafruit SSD1306/GFX/SHT31/SGP30 + claws/BH1750）
└── README.md           # 本文档（仓库根另有 README 索引各板卡项目）
```

## 使用方法

```bash
pio run -d F8266-1              # 编译（在仓库根执行）
pio run -d F8266-1 -t upload    # 烧录到开发板
pio device monitor              # 打开串口监视器（115200）
```

VSCode 中直接打开 `F8266-1` 文件夹即可用 PlatformIO IDE 的构建/烧录按钮。

**预期现象**：烧录上电后，OLED 即点亮首屏（`F8266-1` / `SmartFarm` / `OLED OK`），串口打印 `[oled] SSD1306 已激活`——若屏不亮，检查接线并把 `oled_ctrl.cpp` 的 `OLED_I2C_ADDR` 在 0x3C/0x3D 间切换（OLED 失败不影响后续联网）。SHT30 接好时串口打印 `[sht] SHT30 已检测到（0x44）`，数秒后 OLED 信息区行 0 显示 `T:xx.xC H:xx%`（每 2s 更新读数、串口同步打印）；未接/故障时该行显示 `SHT30 ERR`（每 10s 自动重扫，接上即恢复）。GY-30 接好时串口打印 `[gy30] GY-30(BH1750) 已检测到（0x23）`，行 1 显示 `L:xxxxlux`（手机手电筒照射可见读数明显变化）；未接/故障时显示 `GY30 ERR`（同上自愈）。MQ-2 上电后行 2 先显示 `MQ2 HEAT`（3 分钟预热），预热完成后显示 `G:xx`（清洁空气基线以现场实测为准，用于标定 `MQ2_ALARM_RAW` 阈值）；用打火机未点火放气靠近传感器，读数迅速上升超过阈值即显示 `MQ2 GAS!` 并串口打印 `<<< GAS ALARM`；若预热后长期 `G:0`（AO 断线/未共地）或长期贴满量程（分压失效）显示 `MQ2 ERR`。土壤传感器探头插干土行 3 显示 `SOIL:DRY`，浇水 1s 后切换 `SOIL:WET`（串口打印「判定切换」；浇水边缘来回跳变时状态保持不翻转；当前模块实测低电平=湿润，极性已按此设定）。SGP30 上电行 4 先显示 `SGP30 INIT`（15s 暖机），随后显示 `C:xxxppm V:xxppb`（对传感器哈气/酒精棉靠近，eCO₂ 与 TVOC 显著上升；清洁环境 eCO₂ 常见 400~600ppm 基线）。执行器上电进入安全态，行 5 显示 `P:OFF F:OFF S:90`——当前控制入口（主机命令/自动逻辑）尚未接入，执行器保持安全态属预期；接入后该行实时反映泵/风扇/舵机动作。随后若环境存在 `ESP-TEST` 网络，串口打印连上后的 IP；若不存在，设备变为配置热点 `F8266-1`，手机连上后浏览器访问 <http://192.168.4.1> 可配置网络。连上默认网络后，只要主机 esp32-1 在线（DHCP 动态 IP 亦可，从机会收到其广播宣告自动切换目标地址；串口会打印「广播发现主机: `<IP>`」），串口即打印「上报上线」，主机网页 8000 端口卡变绿显示「F8266-1 已上线」（主机与从机须在同一局域网网段，且 `NET_HOST_ANNOUNCE_PORT` 与主机 `HOST_ANNOUNCE_PORT` 一致）。

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
| 2026-09-08 | 执行实体接入：新增 `actuator_ctrl` 模块（水泵 GPIO13 低触发 / 风扇双路 GPIO12/16 正反转带软件互锁 / 舵机 GPIO0 180°），上电安全态（泵关/扇停/舵机 90°），OLED 行 5 状态上屏（信息区 6 行满员），控制入口待接主机命令/自动逻辑，编译通过 |
| 2026-09-08 | 子系统⑤接入：新增 `sgp30_sensor` 模块（SGP30 空气质量，I2C 0x58 共线，eCO₂ 等效值+TVOC，15s 暖机 + 未做基线持久化）；OLED 布局改造——标题压 1 行（`OLED OK` 退役），信息区 4→6 行，行 4 上屏；依赖新增 Adafruit SGP30，编译通过 |
| 2026-09-07 | 子系统③接入：新增 `mq2_sensor` 模块（MQ-2 可燃气体，ADC A0 模拟量，独立 5V 供电+共地+5×1k/1k 分压，5 采样中值滤波，3 分钟预热管理 + 阈值报警 GAS! + 贴边合理性监测），OLED 行 2 上屏，编译通过 |
| 2026-09-07 | 子系统④接入：新增 `soil_sensor` 模块（土壤湿度 DO→GPIO14，3V3 供电，高=湿润/低=过干 + 1s 环境消抖），OLED 行 3 上屏（信息区 4 行满员），编译通过 |
| 2026-09-06 | 子系统②接入：新增 `bh1750_sensor` 模块（GY-30/BH1750 光照采集，I2C 0x23 共线，连续高分辨率模式，同构子系统①的健康判定/热插拔重扫）；`oled_ctrl` 信息行升级为**信息区 4 行**（`setInfoLine(row, ...)`，每子系统一行）；依赖新增 claws/BH1750，编译通过 |
| 2026-09-06 | 子系统①接入：新增 `sht30_sensor` 模块（SHT30 温湿度采集，I2C 0x44 与 OLED 共线，每 2s 采集 + CRC 校验 + 连续失败判故障 + 10s 热插拔重扫）；`oled_ctrl` 增通用信息行接口 `setInfoLine`，温湿度/故障实时上屏；I2C 引脚集中入 `net_config.h`；依赖新增 Adafruit SHT31，编译通过 |
| 2026-09-05 | 业务层起步：新增 `oled_ctrl` 模块，激活 0.96" SSD1306 OLED（I2C GPIO4/5，地址 0x3C），开机点亮显示设备名 / SmartFarm / OLED OK，作为智慧农场状态可视化层入口；依赖新增 Adafruit SSD1306/GFX，编译通过 |
| 2026-09-05 | 主机 IP 支持 **UDP 广播动态发现**：`host_link` 监听主机宣告（端口 45555）自动采用发现地址，超时回退硬编码 IP；`net_config.h` 增发现参数，README 同步，编译通过 |
| 2026-09-05 | 构建板型由 ESP-01 换为 **Adafruit HUZZAH ESP8266**（`board=huzzah`，4MB Flash），说明文档同步 |
| 2026-09-05 | 接入主机通信层：新增 `host_link` 模块，联网后作为 TCP 客户端连主机 `192.168.1.100:8000`，连上上报 `@F8266-1 online/`，`@…/` 帧收发 + 断线自动重连；命令内容留 `onHostCommand()` 扩展口；主机连接参数进 `net_config.h`，编译通过 |
| 2026-09-04 | 创建项目：WiFiManager STA 联网模块化实现（`net_config.h` + `wifi_net` 模块 + 精简 `main.cpp`），直连 `ESP-TEST/88888888`，同名配置热点 `F8266-1` 兜底 + 断线自愈；加入仓库 sandbox-solution 顶层目录，编译通过 |
