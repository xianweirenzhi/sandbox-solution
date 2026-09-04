#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

// ================= AP 配置 =================
const char *AP_SSID = "ESP8266-Intro";  // 热点名称
// 空密码 = 开放热点，手机连上即弹门户页；如需加密改为 "12345678"（至少 8 位）
const char *AP_PASSWORD = "";

const IPAddress AP_IP(192, 168, 4, 1);        // 网关兼 Web 服务器地址
const IPAddress AP_NETMASK(255, 255, 255, 0);

DNSServer dnsServer;       // 通配 DNS：任意域名都解析到本机，保证门户页能弹出
ESP8266WebServer server(80);

// ================= 介绍页（存于 Flash，不占 RAM；UTF-8 编码）=================
static const char INTRO_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP8266 简介</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
background:linear-gradient(160deg,#0f2027,#203a43,#2c5364);color:#e8f1f2;
min-height:100vh;padding:28px 16px 40px}
.wrap{max-width:640px;margin:0 auto}
header{text-align:center;margin-bottom:26px}
header .chip{font-size:34px}
h1{font-size:26px;letter-spacing:1px;margin-top:6px}
header p{color:#9fb8c8;margin-top:8px;font-size:14px;line-height:1.7}
.card{background:rgba(255,255,255,.06);border:1px solid rgba(255,255,255,.13);
border-radius:14px;padding:18px 20px;margin-bottom:16px}
.card h2{font-size:17px;color:#7fd8ff;margin-bottom:10px}
.card p,.card li{font-size:14px;line-height:1.9;color:#cfe0e8}
.card ul{padding-left:20px}
.spec-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.spec{background:rgba(0,0,0,.28);border-radius:10px;padding:12px 8px;text-align:center}
.spec b{display:block;font-size:17px;color:#ffd166;margin-bottom:2px}
.spec span{font-size:12px;color:#9fb8c8}
footer{text-align:center;color:#7a97a8;font-size:12px;margin-top:22px;line-height:1.9}
</style>
</head>
<body>
<div class="wrap">
<header>
<div class="chip">&#128736;&#65039;</div>
<h1>ESP8266</h1>
<p>你现在看到的这个页面，正由这块 ESP8266 开发板通过 Wi-Fi 直接提供——
没有任何服务器参与。</p>
</header>

<div class="card">
<h2>它是什么？</h2>
<p>ESP8266 是乐鑫（Espressif）于 2014 年推出的 Wi-Fi SoC，片内集成了 32 位处理器
和完整的 TCP/IP 协议栈。凭借超低的价格（几块钱）和丰富的生态，它把联网能力带给了
无数创客和物联网设备，被誉为物联网民主化的功臣。</p>
</div>

<div class="card">
<h2>关键规格</h2>
<div class="spec-grid">
<div class="spec"><b>32 位</b><span>Tensilica L106 CPU</span></div>
<div class="spec"><b>80/160 MHz</b><span>主频可切换</span></div>
<div class="spec"><b>802.11 b/g/n</b><span>2.4GHz Wi-Fi</span></div>
<div class="spec"><b>~50 KB</b><span>用户可用 RAM</span></div>
<div class="spec"><b>17 路 GPIO</b><span>最多可用引脚</span></div>
<div class="spec"><b>~20 µA</b><span>深度睡眠电流</span></div>
</div>
</div>

<div class="card">
<h2>能做什么？</h2>
<ul>
<li>智能家居：灯光、插座、开关的联网控制</li>
<li>传感器网关：温湿度等采集后上报服务器</li>
<li>Web 服务器：正如本页，用浏览器就能控制硬件</li>
<li>网络配网：像你刚刚连上的这个热点一样</li>
</ul>
</div>

<div class="card">
<h2>常见开发板</h2>
<p>NodeMCU、Wemos D1 mini、Adafruit HUZZAH——本机正是 Adafruit HUZZAH ESP8266。</p>
</div>

<footer>本页由 ESP8266 内置 HTTP 服务器提供 · SoftAP 模式 · 192.168.4.1 · 2026</footer>
</div>
</body>
</html>)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", INTRO_PAGE);
  Serial.println("[HTTP] 已发送介绍页 -> " + server.client().remoteIP().toString());
}

// 任意未知路径一律 302 跳转到主页。
// 手机/电脑连上热点后会请求系统探测地址（如 /generate_204、/hotspot-detect.html），
// 收到重定向即判定为"需要登录"，自动弹出内置浏览器打开介绍页 —— 这就是强制门户。
void redirectToRoot() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=== ESP8266 HUZZAH：AP 模式 + 介绍页强制门户 ===");

  // 启动 AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
  if (AP_PASSWORD[0] == '\0') {
    WiFi.softAP(AP_SSID);  // 开放热点
  } else {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  }

  // 通配 DNS：所有域名都解析到自己，保证任意探测请求都能到达
  dnsServer.start(53, "*", AP_IP);

  // Web 服务
  server.on("/", handleRoot);
  server.onNotFound(redirectToRoot);
  server.begin();

  Serial.println("[AP] 热点名: " + String(AP_SSID) + (AP_PASSWORD[0] ? "（加密）" : "（开放）"));
  Serial.println("[AP] 网关地址: " + WiFi.softAPIP().toString());
  Serial.println("[AP] 用手机/电脑连接该热点，将自动弹出 ESP8266 介绍页");
}

void loop() {
  dnsServer.processNextRequest();  // 必须持续处理 DNS，门户页才弹得出来
  server.handleClient();
}
