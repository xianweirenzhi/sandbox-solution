#pragma once

// WifiNet：网络模块对外接口（封装 WiFiManager，STA 模式）。
// 外部（main 等）只需依赖本头文件；实现细节与第三方库包含见 wifi_net.cpp。
// 职责：联网（默认网络直连 + 配置热点兜底）与断线自愈，不掺入任何业务逻辑。

class WifiNet {
 public:
  // 联网：直连默认网络；失败自动开同名配置热点；全程失败超时后自动重启重试。
  // 返回 true 表示已在线（内部失败路径会重启，正常不会返回 false）。
  bool begin();

  // 当前是否已连接上无线网络
  bool isConnected() const;

  // 周期维护：失联超过阈值自动重连（在 main 的 loop 中每轮调用）
  void handle();

 private:
  // 打开配置热点（AP 名 = NET_DEVICE_NAME），供手动改配无线参数
  void openConfigPortal();
};
