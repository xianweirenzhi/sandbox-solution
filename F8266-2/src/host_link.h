#pragma once

// HostLink：F8266-2 从机 → esp32-1 主机（NET_HOST_IP:NET_HOST_PORT）的 TCP 通信模块。
// 职责：WiFi 在线时维持与主机的 TCP 连接，连上即上报上线帧；接收主机命令帧并分发
//       到业务回调；对外提供 send() 发送接口。只做链路，不掺业务逻辑。
// 说明：① 连接对象、收发缓冲等实现细节全部收敛在 .cpp（pimpl），本头文件只暴露接口，
//       不依赖 Arduino / 第三方库（String 仅前置声明，签名中只使用引用）。
//       ② 主机 IP 支持 UDP 广播动态发现：监听到主机宣告（同网段）即自动采用发现地址，
//       超时无宣告则回退 net_config.h 的 NET_HOST_IP（详见 host_link.cpp 的 pollDiscover）。

class String;  // 前置声明：仅用于接口签名（const String&），头文件不包含 Arduino.h

class HostLink {
 public:
  // 业务命令回调：收到主机命令的【内容】（帧头 @ / 帧尾 已剥离）时调用。
  // ★ 命令详细内容在此扩展（由 main 注册具体解析逻辑）。
  using CommandCb = void (*)(const String &cmd);

  HostLink();
  ~HostLink();

  // 复位链路状态（在 net 联网成功后调用一次；主机 IP 自 net_config.h 读取）
  void begin();

  // 周期维护：连接主机 / 收发命令帧 / 断线重连（在 main 的 loop 中每轮调用）
  void handle();

  // 当前是否已连上主机（WiFi 在线且 TCP 已连接）
  bool isOnline() const;

  // 业务发送：把 cmd 自动包成 @cmd/ 帧发往主机。在线且帧内容合法返回 true。
  bool send(const String &cmd);

  // 注册业务命令回调（命令扩展口；不注册则收到命令仅丢弃）
  void onCommand(CommandCb cb);

 private:
  bool writeFrame(const String &payload);  // 写一帧 @payload/ 到当前连接
  void tryConnect();                       // 发起一次连接；成功即上报上线
  void pollDiscover();                     // 监听主机 UDP 广播,动态更新主机 IP
  void pollRx();                           // 读字节流并按 @…/ 切出完整命令帧
  void dispatchCommand();                  // 把收齐的一帧内容交给业务回调

  struct Impl;  // 实现体（定义在 .cpp），避免头文件泄漏连接对象/缓冲
  Impl *_p;
};
