#include <Arduino.h>

#include "net_config.h"   // 设备统一名称（NET_DEVICE_NAME）
#include "wifi_net.h"     // 网络模块对外接口
#include "host_link.h"    // 主机通信模块对外接口

// 全局模块实例（业务模块后续在此按需添加同类实例）
WifiNet net;
HostLink link;  // F8266-1 从机 → esp32-1 主机(NET_HOST_PORT) 的 TCP 链路

// ===================== 业务命令扩展口 =====================
// 收到主机下发命令【内容】时回调这里（帧头 @ / 帧尾 / 已由 host_link 剥离）。
// ★ 命令详细内容留待在此扩展：解析 cmd 并执行对应业务；如需回包请 link.send("...")。
void onHostCommand(const String &cmd) {
  Serial.print(F("[main] 收到主机命令: "));
  Serial.println(cmd);
  // TODO: 命令解析与业务执行（如开灯/关灯、上报状态等）
}

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

  link.begin();                    // 复位主机链路状态
  link.onCommand(onHostCommand);   // 注册业务命令回调（命令扩展口）
  Serial.println(F("[main] 网络就绪，进入主循环"));
}

void loop() {
  net.handle();   // 网络周期维护（断线自愈）
  link.handle();  // 主机链路周期维护（连接/收发/断线重连）

  // ===== 业务模块扩展位 =====
  // 新增功能时请单独建模块文件（如 led_ctrl.h/.cpp），并在上方实例化、
  // 在本循环内调用其 handle()。避免把逻辑堆进 main.cpp，便于长期维护。
  // 命令详细内容请在 onHostCommand() 中扩展。

  delay(10);
}
