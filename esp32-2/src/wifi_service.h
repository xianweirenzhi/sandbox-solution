#pragma once

// 网络工作模式
enum class NetMode {
  STA,          // 已连上路由器,正常工作
  AP_FALLBACK   // 路由器连接超时,AP 直连兜底模式
};

// WiFi 连接管理模块:优先 STA 连接,超时自动切换 AP 兜底
namespace wifi_service {

void begin();             // 初始化并完成模式选择(阻塞,最多 STA_TIMEOUT_MS)
void loop();              // 周期维护(预留:重连策略等)
NetMode mode();           // 当前模式
const char* modeName();   // "STA" / "AP"
bool staConnected();      // 当前是否以 STA 连上了路由器

}  // namespace wifi_service
