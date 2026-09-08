#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>   // ESP8266 的 UDP 类 WiFiUDP（ESP8266WiFi.h 不自动包含它）

#include "net_config.h"
#include "host_link.h"

// ===================== 实现体 =====================
// 连接对象、UDP 发现与收发缓冲集中在此，对外仅暴露 HostLink 接口。
struct HostLink::Impl {
  WiFiClient    client;              // 到主机的 TCP 连接
  IPAddress     host;                // 当前目标主机 IP（动态发现或回退地址）
  IPAddress     fallbackHost;        // 回退主机 IP（由 NET_HOST_IP 解析）
  bool          hostReady = false;   // 回退主机 IP 是否解析成功（配置错则不动作）
  CommandCb     cb = nullptr;        // 业务命令回调（命令扩展口）
  WiFiUDP       udp;                 // UDP 监听：接收主机广播宣告
  bool          sawAnnounce = false; // 是否收到过有效主机宣告
  unsigned long lastSeenMs = 0;      // 上次收到有效宣告时刻（超时回退用）
  char          rx[NET_FRAME_MAX_LEN + 1];  // 帧内容接收缓冲（防御 '\0' 结尾多留 1）
  size_t        rxLen = 0;           // 当前帧已接收字节数
  bool          inFrame = false;     // 正在接收帧内容（已见 @ 未到 /）
  bool          wasOnline = false;   // 上一轮链路是否在线（用于状态切换打印）
  unsigned long lastTryMs = 0;       // 上次尝试连接时刻（限频用）
};

// ===================== 内部小工具 =====================

// 写一帧 @payload/ 到当前连接（仅在线时有效）
bool HostLink::writeFrame(const String &payload) {
  if (!isOnline()) return false;
  // 帧内容不允许为空，也不允许含帧头/帧尾字符，避免破坏 @…/ 帧结构
  if (payload.length() == 0) return false;
  if (payload.indexOf('@') >= 0 || payload.indexOf('/') >= 0) return false;

  String frame = String('@') + payload + '/';
  size_t n = _p->client.write((const uint8_t *)frame.c_str(), frame.length());
  return n == frame.length();
}

// ===================== 公开接口 =====================

HostLink::HostLink() : _p(new Impl) {}

HostLink::~HostLink() {
  delete _p;
  _p = nullptr;
}

void HostLink::begin() {
  _p->hostReady = _p->fallbackHost.fromString(NET_HOST_IP);
  _p->host = _p->fallbackHost;            // 默认先连回退地址,收到宣告后自动切换
  _p->client.stop();
  _p->cb = nullptr;
  _p->sawAnnounce = false;
  _p->lastSeenMs = 0;
  _p->rxLen = 0;
  _p->inFrame = false;
  _p->wasOnline = false;
  // 首次尝试立即触发（减一个重试间隔，使 handle 首轮即尝试连接）
  _p->lastTryMs = millis() - NET_LINK_RETRY_MS;

  // 开始监听主机 UDP 广播宣告（动态发现;仅当有回退地址时才监听）
  if (_p->hostReady) {
    _p->udp.begin(NET_HOST_ANNOUNCE_PORT);
  } else {
    Serial.print(F("[link] NET_HOST_IP 无效: "));
    Serial.println(NET_HOST_IP);
  }
}

bool HostLink::isOnline() const {
  return _p->hostReady && _p->client.connected();
}

bool HostLink::send(const String &cmd) {
  if (!isOnline()) return false;
  return writeFrame(cmd);
}

void HostLink::onCommand(CommandCb cb) {
  _p->cb = cb;
}

void HostLink::handle() {
  // 0) 主机 IP 配置无效时不动作
  if (!_p->hostReady) return;

  // 1) WiFi 不在线：暂停主机链路（wifi_net 断线自愈恢复后会自动重连）
  if (WiFi.status() != WL_CONNECTED) {
    if (_p->client.connected()) {
      _p->client.stop();
      Serial.println(F("[link] WiFi 断开，主机链路暂停"));
    }
    _p->wasOnline = false;
    _p->inFrame = false;
    _p->rxLen = 0;
    return;
  }

  // 1.5) 监听主机广播宣告,动态更新目标主机 IP
  pollDiscover();

  // 动态发现超时:主机离线/换网,回退到硬编码地址重新寻找
  unsigned long now2 = millis();
  if (_p->sawAnnounce && now2 - _p->lastSeenMs > NET_HOST_DISCOVER_STALE_MS) {
    _p->sawAnnounce = false;
    if (_p->host != _p->fallbackHost) {
      Serial.print(F("[link] 主机宣告超时，回退地址: "));
      Serial.println(_p->fallbackHost);
      _p->host = _p->fallbackHost;
      if (_p->client.connected()) {      // 断开旧目标以触发重连
        _p->client.stop();
        _p->wasOnline = false;
        _p->inFrame = false;
        _p->rxLen = 0;
      }
    }
  }

  // 2) 链路断开：打印一次掉线后限频重连
  if (!_p->client.connected()) {
    if (_p->wasOnline) {                 // 由在线 → 断开，刚发生
      Serial.println(F("[link] 与主机连接断开，准备重连"));
      _p->client.stop();                 // 清理旧连接
      _p->wasOnline = false;
      _p->inFrame = false;
      _p->rxLen = 0;
      _p->lastTryMs = millis();          // 先停一个重试周期再连
      return;
    }
    unsigned long now = millis();
    if (now - _p->lastTryMs >= NET_LINK_RETRY_MS) {
      _p->lastTryMs = now;
      tryConnect();
    }
    return;
  }

  // 3) 在线：维持并接收主机命令
  _p->wasOnline = true;
  pollRx();
}

// ===================== 私有实现 =====================

void HostLink::tryConnect() {
  _p->client.stop();
  Serial.print(F("[link] 尝试连接主机 "));
  Serial.print(_p->host);
  Serial.print(':');
  Serial.println(NET_HOST_PORT);

  // 同步连接（核心仅提供无超时重载；同网段主机离线时 SDK 会在数秒内超时返回失败，
  // 且调用频率受 NET_LINK_RETRY_MS 门控，不会拖死主循环）
  if (_p->client.connect(_p->host, NET_HOST_PORT) != 1) {
    Serial.println(F("[link] 连接失败，稍后重试"));
    _p->client.stop();
    return;
  }
  _p->client.setNoDelay(true);   // 关闭 Nagle，命令帧实时到达
  _p->rxLen = 0;
  _p->inFrame = false;

  // 连上即上报上线帧 @F8266-2 online/（主机据此在对应端口标记「已上线」）
  Serial.print(F("[link] 已连上主机，上报上线: @"));
  Serial.print(NET_DEVICE_NAME);
  Serial.println(F(" online/"));
  writeFrame(String(NET_DEVICE_NAME) + " online");
}

void HostLink::pollDiscover() {
  // 主机周期广播形如: ESP32HOST,<ip>[,<base>,<count>] ；从机只取 <ip> 作为目标主机
  int pkt = _p->udp.parsePacket();
  if (pkt <= 0) return;

  char buf[96];
  int n = _p->udp.read(buf, sizeof(buf) - 1);
  if (n <= 0) return;
  buf[n] = '\0';

  const char *prefix = NET_HOST_ANNOUNCE_PREFIX;
  size_t plen = strlen(prefix);
  if (strncmp(buf, prefix, plen) != 0 || buf[plen] != ',') return;

  char *ipstr = buf + plen + 1;
  char *comma = strchr(ipstr, ',');
  if (comma) *comma = '\0';              // 截断到第一个逗号（忽略 base/count）

  IPAddress parsed;
  if (!parsed.fromString(ipstr)) return;

  _p->sawAnnounce = true;
  _p->lastSeenMs = millis();

  if (parsed != _p->host) {              // 主机地址变化：切换目标并断旧连以触发重连
    Serial.print(F("[link] 广播发现主机: "));
    Serial.println(parsed);
    _p->host = parsed;
    if (_p->client.connected()) {
      _p->client.stop();
      _p->wasOnline = false;
      _p->inFrame = false;
      _p->rxLen = 0;
    }
  }
}

void HostLink::pollRx() {
  // 读取本批可用字节，按 @…/ 帧切出完整命令（数据可能分多次到达，需跨调用缓存）
  while (_p->client.available() > 0) {
    int c = _p->client.read();
    if (c < 0) break;
    char ch = (char)c;

    if (ch == '@') {                 // 帧头：重新开始一帧
      _p->rxLen = 0;
      _p->inFrame = true;
    } else if (ch == '/') {          // 帧尾：一帧收齐
      if (_p->inFrame) {
        _p->inFrame = false;
        if (_p->rxLen > 0) dispatchCommand();
        _p->rxLen = 0;
      }                              // 帧外的 / 直接忽略
    } else if (_p->inFrame) {
      if (_p->rxLen < NET_FRAME_MAX_LEN) {
        _p->rx[_p->rxLen++] = ch;
      } else {                       // 超长帧：放弃，等下一个 @ 重新同步
        _p->inFrame = false;
        _p->rxLen = 0;
      }
    }
  }
}

void HostLink::dispatchCommand() {
  _p->rx[_p->rxLen] = '\0';           // 先补串尾，便于交给业务
  if (_p->cb) {
    _p->cb(String(_p->rx));           // 交给业务扩展回调（内容已剥离 @ /）
  }
  _p->rxLen = 0;
}
