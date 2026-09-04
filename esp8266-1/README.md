# ESP8266 开发板测试项目

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-04

## 项目概述

基于 PlatformIO + Arduino 框架的 ESP8266 开发板测试项目。当前固件以 **SoftAP 模式**开放热点，内置一个 ESP8266 介绍网页，设备连接热点后**自动弹出该页面**（强制门户 / captive portal）。

本项目位于仓库 [sandbox-solution](https://github.com/xianweirenzhi/sandbox-solution)（公开）的 `esp8266-1/` 子目录——仓库为多板卡项目集合，其他板卡项目以同级目录存放，详见仓库根 [README](../README.md)。

## 硬件信息

| 项 | 值 |
| --- | --- |
| 开发板 | Adafruit HUZZAH ESP8266 |
| 平台 | espressif8266（PlatformIO） |
| 框架 | Arduino |
| 串口波特率 | 115200 |

## 当前功能

全部实现在 [src/main.cpp](src/main.cpp)：

1. **SoftAP 热点**：开放热点 `ESP8266-Intro`，网关地址 `192.168.4.1`
   - 如需加密：将 `AP_PASSWORD` 改为至少 8 位密码（空密码 = 开放，便于弹门户页）
2. **介绍网页**：内嵌 ESP8266 介绍页（是什么 / 关键规格 / 能做什么 / 常见开发板），以 PROGMEM 存于 Flash，`GET /` 返回
3. **连接自动跳转（强制门户）**：
   - 通配 DNS（`DNSServer`，任意域名 → `192.168.4.1`）
   - 未知路径一律 302 重定向到主页，命中系统探测地址（`/generate_204`、`/hotspot-detect.html` 等）即触发手机/电脑自动弹出内置浏览器
4. **串口日志**：启动时打印热点信息，每次访问打印来源 IP

> 板载 LED 闪烁测试已按需求移除。

## 目录结构

```text
esp8266-1/
├── src/main.cpp        # 主程序（SoftAP + 介绍页强制门户）
├── include/            # 头文件（当前为空）
├── lib/                # 项目私有库（当前为空）
├── test/               # 单元测试（当前为空）
├── platformio.ini      # PlatformIO 配置（环境名 huzzah）
└── README.md           # 本文档（仓库根另有 README 索引各板卡项目）
```

## 使用方法

```bash
pio run -d esp8266-1              # 编译（在仓库根执行）
pio run -d esp8266-1 -t upload    # 烧录到开发板
pio device monitor                # 打开串口监视器（115200）
```

VSCode 中直接打开 `esp8266-1` 文件夹即可用 PlatformIO IDE 的构建/烧录按钮。

**预期现象**：串口打印热点信息后，用手机/电脑连接 `ESP8266-Intro` 热点，系统自动弹出 ESP8266 介绍页；未弹出时手动访问 <http://192.168.4.1> 即可。

## 开发环境

- Windows 11 + VSCode（PlatformIO IDE 扩展）
- PlatformIO Core（`~/.platformio/penv/Scripts/pio.exe`）

## 更新记录

| 日期 | 变更内容 |
| --- | --- |
| 2026-09-01 | 创建项目；实现 LED 闪烁 + 串口测试程序，编译通过 |
| 2026-09-01 | 建立 Git 仓库并推送至 GitHub（sandbox-solution，公开），此后代码变更同步推送 |
| 2026-09-03 | 改为 SoftAP + 强制门户：开放热点、内嵌 ESP8266 介绍页、连接自动跳转；按需求移除板载 LED 功能，编译通过 |
| 2026-09-04 | 仓库改为多板卡结构，本项目迁入 `esp8266-1/` 子目录；platformio.ini 补提交 WiFiManager 依赖，编译通过 |
