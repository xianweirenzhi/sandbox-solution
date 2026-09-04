#include "wifi_service.h"

#include <WiFi.h>

#include "config.h"

namespace wifi_service {

static NetMode s_mode = NetMode::AP_FALLBACK;  // begin() 之前视为未连接

NetMode mode() { return s_mode; }

const char* modeName() { return s_mode == NetMode::STA ? "STA" : "AP"; }

bool staConnected() {
  return s_mode == NetMode::STA && WiFi.status() == WL_CONNECTED;
}

// 以 STA 连接目标 AP,阻塞直到成功或超时
static bool connectSTA() {
  Serial.printf("[WiFi] 正在连接路由器 \"%s\"(最长等待 %lus)...\n",
                STA_SSID, STA_TIMEOUT_MS / 1000);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);   // 掉线后自动重连
  // 固定本机 IP,便于从机定位主机地址(须与路由器同网段,见 config.h)
  WiFi.config(STA_STATIC_IP, STA_GATEWAY, STA_NETMASK, STA_DNS);
  Serial.printf("[WiFi] 使用固定 IP: %s\n", STA_STATIC_IP.toString().c_str());
  WiFi.begin(STA_SSID, STA_PASS);

  uint32_t t0 = millis();
  uint32_t lastLog = 0;
  while (WiFi.status() != WL_CONNECTED) {
    uint32_t elapsed = millis() - t0;
    if (elapsed >= STA_TIMEOUT_MS) return false;
    if (elapsed - lastLog >= 10000) {          // 每 10 秒打印剩余等待时间
      lastLog = elapsed;
      Serial.printf("[WiFi] 连接中... 剩余 %lus\n",
                    (STA_TIMEOUT_MS - elapsed) / 1000);
    }
    delay(200);
  }
  return true;
}

// 启动 AP 兜底模式:手机直连热点访问设备网页
static void startFallbackAP() {
  WiFi.mode(WIFI_AP);  // 切换为纯 AP 模式
  WiFi.softAPConfig(FALLBACK_AP_IP, FALLBACK_AP_IP, FALLBACK_AP_MASK);
  WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASS);
  s_mode = NetMode::AP_FALLBACK;

  Serial.printf("[WiFi] %lus 内未能连接 \"%s\",已切换 AP 直连模式\n",
                STA_TIMEOUT_MS / 1000, STA_SSID);
}

void begin() {
  if (connectSTA()) {
    s_mode = NetMode::STA;
    return;
  }
  startFallbackAP();   // STA 超时 -> AP 兜底(重启设备才会重新尝试 STA)
}

void loop() {
  // 预留:STA 掉线重连上报、AP 模式下周期重试 STA 等扩展点
}

}  // namespace wifi_service
