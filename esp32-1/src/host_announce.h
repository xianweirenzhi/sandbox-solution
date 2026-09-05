#pragma once

// 主机广播宣告:STA 在线时周期向局域网 UDP 广播自身 IP 与端口范围,
// 供从机 F8266-x 动态发现主机(即使主机为 DHCP 动态 IP 也能找到)。
namespace host_announce {

void begin();   // 初始化
void loop();    // 周期广播,需在主循环中调用

}
