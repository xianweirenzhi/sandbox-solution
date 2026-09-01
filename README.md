# ESP8266 开发板测试项目

> 本文档由 Claude Code 维护，随项目变更同步更新。最后更新：2026-09-01

## 项目概述

基于 PlatformIO + Arduino 框架的 ESP8266 开发板测试项目，用于验证开发板基本功能（GPIO 输出、串口通信）。

**GitHub 仓库**：<https://github.com/xianweirenzhi/sandbox-solution> （公开，代码与此目录同步）

## 硬件信息

| 项 | 值 |
| --- | --- |
| 开发板 | Adafruit HUZZAH ESP8266 |
| 平台 | espressif8266（PlatformIO） |
| 框架 | Arduino |
| 板载 LED | 红色 LED，接 GPIO0，**低电平点亮** |
| 串口波特率 | 115200 |

## 当前功能

1. **LED 闪烁测试**（[src/main.cpp](src/main.cpp)）：GPIO0 每 500ms 翻转一次，验证 GPIO 输出正常
2. **串口输出测试**：同步打印 `[LED] ON` / `[LED] OFF`，验证 USB 串口通信正常

## 目录结构

```text
esp8266/
├── src/main.cpp        # 主程序（LED 闪烁 + 串口测试）
├── include/            # 头文件（当前为空）
├── lib/                # 项目私有库（当前为空）
├── test/               # 单元测试（当前为空）
├── platformio.ini      # PlatformIO 配置（环境名 huzzah）
└── README.md           # 本文档
```

## 使用方法

```bash
pio run                  # 编译
pio run -t upload        # 烧录到开发板
pio device monitor       # 打开串口监视器（115200）
```

**预期现象**：板上红灯每秒闪烁一次，串口监视器同步打印 `[LED] ON` / `[LED] OFF`。

## 开发环境

- Windows 11 + VSCode（PlatformIO IDE 扩展）
- PlatformIO Core（`~/.platformio/penv/Scripts/pio.exe`）

## 更新记录

| 日期 | 变更内容 |
| --- | --- |
| 2026-09-01 | 创建项目；实现 LED 闪烁 + 串口测试程序，编译通过 |
| 2026-09-01 | 建立 Git 仓库并推送至 GitHub（sandbox-solution，公开），此后代码变更同步推送 |
