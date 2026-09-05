#include "host_announce.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"

namespace host_announce {

static WiFiUDP s_udp;
static uint32_t s_lastMs = 0;

// 向指定目标发一条宣告: ESP32HOST,<本机IP>,<起始端口>,<端口数>
static void broadcastTo(const IPAddress &dest) {
  char payload[64];
  snprintf(payload, sizeof(payload), "%s,%s,%u,%u",
           HOST_ANNOUNCE_PREFIX, WiFi.localIP().toString().c_str(),
           (unsigned)PORT_BASE, (unsigned)PORT_COUNT);
  s_udp.beginPacket(dest, HOST_ANNOUNCE_PORT);
  s_udp.write((const uint8_t *)payload, strlen(payload));
  s_udp.endPacket();
}

void begin() { s_lastMs = 0; }

void loop() {
  // 仅 STA 在线(有局域网路由)时宣告;AP 兜底模式无路由器从机,跳过
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - s_lastMs < HOST_ANNOUNCE_INTERVAL_MS) return;
  s_lastMs = now;

  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  // 定向广播: ip | ~mask (如 192.168.1.255)
  IPAddress bcast;
  for (int i = 0; i < 4; i++) bcast[i] = (uint8_t)(ip[i] | (~mask[i] & 0xFF));
  broadcastTo(bcast);
  broadcastTo(IPAddress(255, 255, 255, 255));  // 有限广播兜底
}

}  // namespace host_announce
