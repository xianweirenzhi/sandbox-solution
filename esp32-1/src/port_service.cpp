#include "port_service.h"

namespace port_service {

// 每个通信端口的运行上下文
struct PortCtx {
  uint16_t    port = 0;
  WiFiServer *server = nullptr;
  WiFiClient  clients[PORT_CLIENTS_MAX];   // 已接入的从机连接
  String      rxPartial;                   // 未遇到换行的半行数据
  String      log[PORT_LOG_LINES];         // 收发记录环形缓冲
  uint8_t     logHead = 0, logLen = 0;
  uint32_t    rxCount = 0, txCount = 0;
  String      slave;                       // 已上线的从机名(F8266-x)
  bool        online = false;
};

static PortCtx s_ports[PORT_COUNT];

// 运行时间戳 HH:MM:SS(基于 millis,未做网络校时)
static String timeStamp() {
  uint32_t s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
           (unsigned)(s / 3600), (unsigned)((s % 3600) / 60), (unsigned)(s % 60));
  return String(buf);
}

static void pushLog(PortCtx &p, const String &line) {
  p.log[p.logHead] = line;
  p.logHead = (p.logHead + 1) % PORT_LOG_LINES;
  if (p.logLen < PORT_LOG_LINES) p.logLen++;
}

static uint8_t countClients(PortCtx &p) {
  uint8_t n = 0;
  for (auto &c : p.clients)
    if (c && c.connected()) n++;
  return n;
}

// 识别从机上线报文 "F8266-x online",x 为从机编号
static void parseOnline(PortCtx &p, const String &line) {
  int id = 0;
  if (sscanf(line.c_str(), "F8266-%d online", &id) == 1 && id > 0 && id < 100) {
    p.slave = "F8266-" + String(id);
    p.online = true;
    pushLog(p, timeStamp() + " ★ " + p.slave + " 已上线");
  }
}

// 接入新从机
static void acceptClients(PortCtx &p) {
  while (WiFiClient c = p.server->available()) {
    bool placed = false;
    for (auto &slot : p.clients) {
      if (!(slot && slot.connected())) {  // 复用空槽位
        slot = c;
        placed = true;
        break;
      }
    }
    if (placed) {
      pushLog(p, timeStamp() + " ● 从机接入 " + c.remoteIP().toString());
    } else {                              // 超出每端口上限,拒绝
      c.stop();
      pushLog(p, timeStamp() + " ✕ 拒绝接入(端口已满) " + c.remoteIP().toString());
    }
  }
}

// 读取一行行报文;检测从机断开
static void readClients(PortCtx &p) {
  for (auto &slot : p.clients) {
    if (!slot) continue;
    if (!slot.connected()) {              // 从机断开
      slot.stop();
      if (countClients(p) == 0 && p.online) {
        p.online = false;                 // 端口已无任何连接 -> 从机离线
        pushLog(p, timeStamp() + " ○ " + (p.slave.length() ? p.slave : String("从机")) + " 已离线");
      }
      continue;
    }
    while (slot.available() > 0) {
      char ch = slot.read();
      if (ch == '\n') {                   // 收到完整一行报文
        String line = p.rxPartial;
        p.rxPartial = "";
        line.trim();
        if (line.length()) {
          p.rxCount++;
          parseOnline(p, line);
          pushLog(p, timeStamp() + " ← " + line);
        }
      } else if (p.rxPartial.length() < 256) {  // 超长行截断保护
        p.rxPartial += ch;
      }
    }
  }
}

void begin() {
  for (int i = 0; i < PORT_COUNT; i++) {
    PortCtx &p = s_ports[i];
    p.port = PORT_BASE + i;
    p.server = new WiFiServer(p.port);
    p.server->begin();
    p.server->setNoDelay(true);
  }
  Serial.printf("[Port] 已开启 %d 个从机通信端口: %u ~ %u\n",
                PORT_COUNT, PORT_BASE, PORT_BASE + PORT_COUNT - 1);
}

void loop() {
  for (auto &p : s_ports) {
    acceptClients(p);
    readClients(p);
  }
}

bool send(uint8_t idx, const String &text) {
  if (idx >= PORT_COUNT) return false;
  PortCtx &p = s_ports[idx];
  uint8_t sent = 0;
  for (auto &slot : p.clients) {
    if (slot && slot.connected()) {
      slot.print(text);
      slot.print('\n');                   // 行式协议,自动补换行
      sent++;
    }
  }
  if (sent > 0) {
    p.txCount++;
    pushLog(p, timeStamp() + " → " + text);
  }
  return sent > 0;                        // false = 该端口当前无在线从机
}

PortSnapshot snapshot(uint8_t idx) {
  PortSnapshot s;
  if (idx >= PORT_COUNT) return s;
  PortCtx &p = s_ports[idx];
  s.port = p.port;
  s.clients = countClients(p);
  s.online = p.online;
  s.slave = p.slave;
  s.rxCount = p.rxCount;
  s.txCount = p.txCount;
  for (uint8_t i = 0; i < p.logLen; i++) {  // 环形缓冲按时间序导出
    uint8_t k = (p.logHead + PORT_LOG_LINES - p.logLen + i) % PORT_LOG_LINES;
    s.log[i] = p.log[k];
  }
  s.logLen = p.logLen;
  return s;
}

}  // namespace port_service
