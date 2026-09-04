#include <Arduino.h>

#include "net_config.h"   // 设备统一名称（NET_DEVICE_NAME）
#include "wifi_net.h"     // 网络模块对外接口

// 全局网络模块实例（业务模块后续在此按需添加同类实例）
WifiNet net;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.print(F("[main] "));
  Serial.print(NET_DEVICE_NAME);
  Serial.println(F(" 启动"));

  if (!net.begin()) {
    // begin() 内部失败会自重启，正常情况下不会走到这里（防御性处理）
    Serial.println(F("[main] 联网失败，重启"));
    ESP.restart();
  }
  Serial.println(F("[main] 网络就绪，进入主循环"));
}

void loop() {
  net.handle();  // 网络周期维护（断线自愈）

  // ===== 业务模块扩展位 =====
  // 新增功能时请单独建模块文件（如 led_ctrl.h/.cpp），并在上方实例化、
  // 在本循环内调用其 handle()。避免把逻辑堆进 main.cpp，便于长期维护。
  // 例：与 esp32-1 主机（F8266-x 端口）的 TCP 通信等后续功能在此接入。

  delay(10);
}
