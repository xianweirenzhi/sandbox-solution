#pragma once

// 网页服务模块:设备状态页面 + JSON 接口(STA/AP 两种模式下均可访问)
namespace web_ui {

void begin();   // 注册路由并启动 HTTP 服务与 mDNS
void loop();    // 处理客户端请求,需在主循环中调用

}
