#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include "net_config.h"
#include "wifi_net.h"

// ===================== 内部小工具 =====================

// 打印当前联网状态（串口日志）
static void printStatus(const char *tag) {
  Serial.print(F("[net] "));
  Serial.print(tag);
  Serial.print(F(" | SSID="));
  Serial.print(WiFi.SSID());
  Serial.print(F(" | IP="));
  Serial.print(WiFi.localIP());
  Serial.print(F(" | hostname="));
  Serial.println(WiFi.hostname());
}

// ===================== 公开接口 =====================

bool WifiNet::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiNet::begin() {
  WiFi.mode(WIFI_STA);                 // STA 模式
  WiFi.hostname(NET_DEVICE_NAME);      // STA 主机名 = F8266-2
  WiFi.setAutoReconnect(true);         // 会话内掉线由 SDK 自动重连

  // 1) 直连硬编码默认网络
  Serial.print(F("[net] 尝试直连默认网络: "));
  Serial.println(NET_DEFAULT_SSID);
  WiFi.begin(NET_DEFAULT_SSID, NET_DEFAULT_PASSWORD);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - t0) < NET_CONNECT_TIMEOUT_MS) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();

  if (isConnected()) {
    printStatus("直连成功");
    return true;
  }

  // 2) 默认网络不可达：打开同名配置热点，供手动改配
  openConfigPortal();
  printStatus("配置热点流程结束");
  return isConnected();
}

void WifiNet::handle() {
  static uint32_t lastOnline = millis();

  if (isConnected()) {                 // 在线：刷新计时即可
    lastOnline = millis();
    return;
  }

  // 失联超过 15s（SDK 自动重连也未恢复）→ 主动重连默认网络 / 配置热点
  if (millis() - lastOnline > 15000UL) {
    Serial.println(F("[net] 长时间失联，主动重连..."));
    begin();
    lastOnline = millis();
  }
}

// ===================== 私有实现 =====================

void WifiNet::openConfigPortal() {
  Serial.println(F("[net] 默认网络不可达，启动配置热点："));
  Serial.print(F("[net] 热点名="));
  Serial.println(NET_DEVICE_NAME);
  Serial.println(F("[net] 手机连接该热点后，浏览器访问 http://192.168.4.1 即可配置无线网络"));

  WiFiManager wm;                      // 模块内局部使用，尽量缩小作用域
  wm.setConnectTimeout(10);            // 配网后连接超时（秒）
  wm.setConfigPortalTimeout(NET_PORTAL_TIMEOUT_S);  // 热点最长停留时长

  if (!wm.autoConnect(NET_DEVICE_NAME)) {  // 热点名 = F8266-2
    Serial.println(F("[net] 配置热点超时且未配网，系统重启重试"));
    ESP.restart();
  }
}
