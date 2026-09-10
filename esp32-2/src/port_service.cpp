#include "port_service.h"

#include <ArduinoJson.h>
#include <errno.h>
#include <lwip/sockets.h>   // recv / MSG_DONTWAIT / EWOULDBLOCK

namespace port_service {

// 帧协议约定: '@' 为帧头, '/' 为帧尾, 两者之间为命令有效内容
struct PortCtx {
  uint16_t    port = 0;
  WiFiServer *server = nullptr;
  WiFiClient  clients[PORT_CLIENTS_MAX];   // 已接入的从机连接
  String      rxFrame;                     // '@' 与 '/' 之间累积的命令内容
  bool        inFrame = false;             // 是否已收到帧头 '@'
  uint32_t    rxLastByteMs = 0;            // 上次收到字节时间(不完整帧超时兜底)
  String      log[PORT_LOG_LINES];         // 收发记录环形缓冲
  uint8_t     logHead = 0, logLen = 0;
  uint32_t    rxCount = 0, txCount = 0;
  String      slave;                       // 已上线的从机名(F8266-x)
  bool        online = false;
  // 从机上报的最新传感数据
  bool        hasData = false;
  float       t = NAN, h = NAN, lux = NAN, co2 = NAN, tvoc = NAN;
  int16_t     soil = -1;             // -1=无;两态口 0=干/1=湿;原始值口 0~1023 低=湿高=干(见 config.h PORT_SOIL_RAW)
  // 从机上报的执行器实时状态
  uint8_t     pump = 0, fan = 0, servo = 90;
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

// 统计当前有效的连接数(以 fd 是否有效为准,避免 connected() 的 errno 副作用)
static uint8_t countClients(PortCtx &p) {
  uint8_t n = 0;
  for (auto &c : p.clients)
    if (c.fd() >= 0) n++;
  return n;
}

// 识别从机上线报文 "F8266-x online",x 为从机编号
static void parseOnline(PortCtx &p, const String &cmd) {
  int id = 0;
  if (sscanf(cmd.c_str(), "F8266-%d online", &id) == 1 && id > 0 && id < 100) {
    p.slave = "F8266-" + String(id);
    p.online = true;
    pushLog(p, timeStamp() + " ★ " + p.slave + " 已上线");
  }
}

// 解析从机传感数据 JSON 帧(如 {"t":25.3,"h":56.0,...})，null/缺失字段置 NaN
static void parseData(PortCtx &p, const String &cmd) {
  JsonDocument doc;
  if (deserializeJson(doc, cmd)) return;   // 解析失败(非数据帧/损坏)忽略
  // 数据帧不逐条刷日志(高频 2s 一次会淹没日志)；仅在首条打印一次
  if (!p.hasData) pushLog(p, timeStamp() + " ⚙ 开始接收传感数据");
  p.hasData = true;
  p.t    = doc["t"].is<float>()    ? doc["t"].as<float>()    : NAN;
  p.h    = doc["h"].is<float>()    ? doc["h"].as<float>()    : NAN;
  p.lux  = doc["lux"].is<float>()  ? doc["lux"].as<float>()  : NAN;
  p.soil = doc["soil"].is<int>()   ? doc["soil"].as<int>()   : -1;  // 两态 0/1 或原始值 0~1023,按端口模式区分(PORT_SOIL_RAW)
  p.co2  = doc["co2"].is<float>()  ? doc["co2"].as<float>()  : NAN;
  p.tvoc = doc["tvoc"].is<float>() ? doc["tvoc"].as<float>() : NAN;
  // 执行器状态:缺失字段保留旧值(旧固件兼容)
  if (doc["pump"].is<int>())  p.pump  = doc["pump"].as<int>();
  if (doc["fan"].is<int>())   p.fan   = doc["fan"].as<int>();
  if (doc["servo"].is<int>()) p.servo = doc["servo"].as<int>();
}

// 端口内无连接时,若曾上线则标记离线
static void markOfflineIfEmpty(PortCtx &p) {
  if (countClients(p) == 0 && p.online) {
    p.online = false;
    pushLog(p, timeStamp() + " ○ " +
                        (p.slave.length() ? p.slave : String("从机")) + " 已离线");
  }
}

// 处理完整一帧(帧头 '@' 与帧尾 '/' 之间)的命令
static void handleCommand(PortCtx &p) {
  String cmd = p.rxFrame;
  p.rxFrame = "";
  p.inFrame = false;
  cmd.trim();
  if (!cmd.length()) return;
  p.rxCount++;
  if (cmd[0] == '{') {          // JSON 数据帧：解析存储，不逐条刷日志(高频 2s)
    parseData(p, cmd);
    return;
  }
  parseOnline(p, cmd);
  pushLog(p, timeStamp() + " ← @" + cmd + "/");
}

// 不完整帧兜底:帧内数据静默超过 PORT_RX_TIMEOUT_MS 即视为帧结束
static void flushRxIfTimeout(PortCtx &p) {
  if (p.inFrame && p.rxFrame.length() > 0 &&
      (millis() - p.rxLastByteMs) >= PORT_RX_TIMEOUT_MS) {
    handleCommand(p);
  }
}

// 调优 TCP keepalive 定时,使从机异常掉电在短时间内被判离线(需平台 LWIP_TCP_KEEPALIVE=1)
static void enableKeepalive(int fd) {
  if (fd < 0) return;
  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
  int val = PORT_KEEPALIVE_IDLE_S;
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val));
  val = PORT_KEEPALIVE_INTVL_S;
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val));
  val = PORT_KEEPALIVE_CNT;
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val));
}

// 接入新从机
static void acceptClients(PortCtx &p) {
  while (WiFiClient c = p.server->available()) {
    bool placed = false;
    for (auto &slot : p.clients) {
      if (slot.fd() < 0) {  // 空槽位(fd 无效)
        slot = c;
        placed = true;
        break;
      }
    }
    if (placed) {
      enableKeepalive(c.fd());  // 掉电探测(半开连接约 IDLE+INTVL*CNT 秒后中止)
      pushLog(p, timeStamp() + " ● 从机接入 " + c.remoteIP().toString());
      Serial.printf("[Port %u] 接入 fd=%d ip=%s\n", p.port, c.fd(),
                    c.remoteIP().toString().c_str());
    } else {  // 超出每端口上限,拒绝
      c.stop();
      pushLog(p, timeStamp() + " ✕ 拒绝接入(端口已满) " + c.remoteIP().toString());
    }
  }
}

// 用底层 recv 直读 + 帧状态机解析;以 recv 返回值判断断开与无数据
static void readClients(PortCtx &p) {
  for (auto &slot : p.clients) {
    int fd = slot.fd();
    if (fd < 0) continue;

    while (true) {
      char buf[128];
      int n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
      if (n > 0) {                        // 收到数据,按帧解析
        p.rxLastByteMs = millis();
        for (int i = 0; i < n; i++) {
          char ch = buf[i];
          if (ch == '@') {                // 帧头:开始新帧(丢弃此前的不完整数据)
            p.inFrame = true;
            p.rxFrame = "";
          } else if (ch == '/') {         // 帧尾:结束当前帧
            if (p.inFrame) handleCommand(p);
          } else if (p.inFrame) {         // 帧内有效内容
            if (p.rxFrame.length() < 256) p.rxFrame += ch;  // 超长截断保护
          }
        }
        continue;                         // 可能还有更多数据
      } else if (n == 0) {                // 对端关闭连接
        slot.stop();
        markOfflineIfEmpty(p);
        break;
      } else {                            // n < 0
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          break;                          // 无更多数据,正常
        }
        slot.stop();                      // 其他错误,关闭
        markOfflineIfEmpty(p);
        break;
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
    flushRxIfTimeout(p);   // 不完整帧的超时兜底
  }
}

bool send(uint8_t idx, const String &text) {
  if (idx >= PORT_COUNT) return false;
  PortCtx &p = s_ports[idx];
  uint8_t sent = 0;
  for (auto &slot : p.clients) {
    if (slot.fd() >= 0) {
      slot.print('@');                    // 帧头
      slot.print(text);                   // 命令有效内容
      slot.print('/');                    // 帧尾
      sent++;
    }
  }
  if (sent > 0) {
    p.txCount++;
    pushLog(p, timeStamp() + " → @" + text + "/");
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
  s.hasData = p.hasData;
  s.t = p.t; s.h = p.h; s.lux = p.lux; s.soil = p.soil;
  s.co2 = p.co2; s.tvoc = p.tvoc;
  s.pump = p.pump; s.fan = p.fan; s.servo = p.servo;
  for (uint8_t i = 0; i < p.logLen; i++) {  // 环形缓冲按时间序导出
    uint8_t k = (p.logHead + PORT_LOG_LINES - p.logLen + i) % PORT_LOG_LINES;
    s.log[i] = p.log[k];
  }
  s.logLen = p.logLen;
  return s;
}

}  // namespace port_service
