/**
 * ESP32-S3 STA/AP 双模式设备入口
 *
 * 流程:
 *   1. 上电以 STA 模式连接路由器 ESP-TEST(最长 2 分钟)
 *   2. 连接成功 -> 正常 STA 模式,同局域网访问设备 IP 打开状态页
 *   3. 连接超时 -> 自动切换 AP 直连模式(热点 ESP32-Direct),
 *      手机连接该热点后访问 192.168.4.1 打开同一状态页
 *
 * 模块划分(便于维护与扩展):
 *   config.h        全部可调参数
 *   wifi_service.*  WiFi 连接与模式切换
 *   device_status.* 状态采集(网页/串口共用)
 *   web_ui.*        网页服务
 */
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "device_status.h"
#include "web_ui.h"
#include "wifi_service.h"

// 串口打印当前连接详情(两种模式共用)
static void printConnectionInfo(const DeviceStatus &s) {
  Serial.println();
  if (s.connected) {
    Serial.println("========= WiFi STA 连接成功 =========");
    Serial.print("模式     : ");
    Serial.println(s.mode);
    Serial.print("SSID     : ");
    Serial.println(s.ssid);
    Serial.print("IP 地址  : ");
    Serial.println(s.ip);
    Serial.print("网关     : ");
    Serial.println(s.gw);
    Serial.print("子网掩码 : ");
    Serial.println(s.mask);
    Serial.print("MAC 地址 : ");
    Serial.println(s.mac);
    Serial.print("信号强度 : ");
    Serial.print(s.rssi);
    Serial.println(" dBm");
    Serial.print("信道     : ");
    Serial.println(s.channel);
  } else {
    Serial.println("========= AP 直连兜底模式 =========");
    Serial.printf("热点名称 : %s\n", FALLBACK_AP_SSID);
    Serial.printf("热点密码 : %s\n", FALLBACK_AP_PASS);
    Serial.print("设备 IP  : ");
    Serial.println(s.ip);
    Serial.println("提示     : 手机连接该热点后,浏览器访问上述 IP 打开状态页");
    Serial.printf("重启设备将重新尝试连接 \"%s\"\n", STA_SSID);
  }
  Serial.println("=====================================");
  Serial.println();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);  // 等待 USB 串口就绪
  Serial.println();
  Serial.println("===== ESP32-S3 STA/AP 双模式设备 =====");

  wifi_service::begin();               // STA 优先,2 分钟超时自动切 AP

  printConnectionInfo(collectStatus());
  web_ui::begin();                     // 两种模式下网页服务均可用
}

void loop() {
  wifi_service::loop();
  web_ui::loop();

  // 每 5 秒串口心跳,持续验证连接状态
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    DeviceStatus s = collectStatus();
    if (s.connected) {
      Serial.printf("[在线] SSID:%s  IP:%s  RSSI:%d dBm  信道:%d\n",
                    s.ssid.c_str(), s.ip.c_str(), s.rssi, s.channel);
    } else if (s.mode == "AP") {
      Serial.printf("[AP] 热点:%s  IP:%s  已连接设备:%d 台\n",
                    s.ssid.c_str(), s.ip.c_str(),
                    WiFi.softAPgetStationNum());
    } else {
      Serial.println("[离线] WiFi 已断开,等待自动重连 ...");
    }
  }
}
