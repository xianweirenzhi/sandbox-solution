# sandbox-solution

> 多板卡固件项目集合。本文档由 Claude Code 维护，随仓库变更同步更新。最后更新：2026-09-04

## 仓库结构

每块开发板 / 每个实验项目独立一个顶层目录（`<板卡>-<序号>`），互不干扰，避免上传冲突：

| 目录 | 板卡 | 说明 |
| --- | --- | --- |
| [esp8266-1/](esp8266-1/) | Adafruit HUZZAH ESP8266 | SoftAP 开放热点 + 内置 ESP8266 介绍页 + 连接自动跳转（强制门户） |
| [esp32-1/](esp32-1/) | 4D Systems gen4-ESP32S3 (R8N16) | STA 优先连接路由器（2 分钟超时自动切 AP 直连兜底）+ 设备状态网页 |

> 新板卡项目请在仓库根新建同级目录（如 `esp32-1/`），并在此表登记。

## 开发环境

- Windows 11 + VSCode（PlatformIO IDE 扩展）
- PlatformIO Core（`~/.platformio/penv/Scripts/pio.exe`）

```bash
pio run -d esp8266-1              # 构建指定项目
pio run -d esp8266-1 -t upload    # 烧录指定项目
```

## 更新记录

| 日期 | 变更内容 |
| --- | --- |
| 2026-09-01 | 建立 Git 仓库并推送至 GitHub，初始为单项目结构 |
| 2026-09-04 | 仓库改造为多板卡项目集合结构：esp8266 项目迁入 `esp8266-1/`，新增仓库级 README |
| 2026-09-04 | 新增 `esp32-1/`：ESP32-S3 STA/AP 双模式设备状态服务（STA 优先 + 超时 AP 兜底 + 状态网页），新位置编译通过 |
