#include "device_status.h"

#include <WiFi.h>

#include "config.h"
#include "wifi_service.h"

// 运行时间格式化:Xd HH:MM:SS
static String formatUptime() {
  uint32_t sec = millis() / 1000;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(sec / 86400),
           (unsigned long)((sec % 86400) / 3600),
           (unsigned long)((sec % 3600) / 60),
           (unsigned long)(sec % 60));
  return String(buf);
}

DeviceStatus collectStatus() {
  DeviceStatus s;

  // ---------- 网络(按工作模式取值) ----------
  s.mode = wifi_service::modeName();
  s.connected = wifi_service::staConnected();
  s.mac = WiFi.macAddress();
  s.hostname = WiFi.getHostname();

  if (wifi_service::mode() == NetMode::STA) {
    s.ssid = WiFi.SSID();
    s.ip = WiFi.localIP().toString();
    s.gw = WiFi.gatewayIP().toString();
    s.mask = WiFi.subnetMask().toString();
    s.rssi = WiFi.RSSI();
    s.channel = WiFi.channel();
  } else {                       // AP 兜底模式
    s.ssid = WiFi.softAPSSID();
    s.ip = WiFi.softAPIP().toString();
    s.gw = s.ip;                 // AP 模式下设备自己就是网关
    s.mask = "255.255.255.0";    // 与 FALLBACK_AP_MASK 保持一致
    s.rssi = 0;                  // AP 模式下无意义
    s.channel = 0;
  }

  // ---------- 系统 ----------
  s.uptime = formatUptime();
  s.heapFree = ESP.getFreeHeap() / 1024;
  s.heapSize = ESP.getHeapSize() / 1024;
  s.psramFree = ESP.getFreePsram() / 1024;
  s.psramSize = ESP.getPsramSize() / 1024;
  s.flashMB = ESP.getFlashChipSize() / (1024UL * 1024UL);
  s.flashMHz = ESP.getFlashChipSpeed() / 1000000UL;
  s.chip = String(ESP.getChipModel()) + " rev" + ESP.getChipRevision() +
           " x" + ESP.getChipCores() + " 核";
  s.cpuMHz = getCpuFrequencyMhz();
  char temp[8];
  snprintf(temp, sizeof(temp), "%.1f", temperatureRead());
  s.tempC = temp;
  s.sdk = ESP.getSdkVersion();

  return s;
}
