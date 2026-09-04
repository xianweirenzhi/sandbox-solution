#include "web_ui.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "device_status.h"
#include "wifi_service.h"

namespace web_ui {

static WebServer server(WEB_PORT);

/* ---------------- 首页 HTML(存于 Flash,节省 RAM) ---------------- */
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 设备状态</title>
<style>
*{box-sizing:border-box}
body{font-family:"Microsoft YaHei",system-ui,sans-serif;background:#f2f3f5;color:#333;margin:0;padding:16px}
.wrap{max-width:480px;margin:0 auto}
h1{font-size:20px;text-align:center;margin:8px 0 16px}
.card{background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.08);padding:14px 18px;margin-bottom:14px}
.card h2{font-size:15px;color:#666;margin:0 0 8px;font-weight:600}
table{width:100%;border-collapse:collapse;font-size:14px}
td{padding:7px 0;border-bottom:1px dashed #eee}
tr:last-child td{border-bottom:none}
td.k{color:#888}
td.v{text-align:right;font-weight:600;word-break:break-all}
.badge{display:flex;align-items:center;justify-content:space-between;padding:4px 0 10px}
.dot{width:10px;height:10px;border-radius:50%;background:#ccc;display:inline-block;margin-right:6px}
.on .dot{background:#2ecc71}
.ap .dot{background:#f39c12}
.off .dot{background:#e74c3c}
.badge .t{font-size:14px;font-weight:600}
footer{text-align:center;color:#aaa;font-size:12px;margin-top:4px}
</style>
</head>
<body>
<div class="wrap">
<h1>ESP32-S3 设备状态</h1>

<div class="card">
  <div class="badge" id="stateBox">
    <span class="t"><span class="dot"></span><span id="stateText">连接中…</span></span>
    <span id="host" style="color:#888"></span>
  </div>
  <table>
    <tr><td class="k">工作模式</td><td class="v" id="mode">--</td></tr>
    <tr><td class="k">SSID</td><td class="v" id="ssid">--</td></tr>
    <tr><td class="k">IP 地址</td><td class="v" id="ip">--</td></tr>
    <tr><td class="k">网关</td><td class="v" id="gw">--</td></tr>
    <tr><td class="k">子网掩码</td><td class="v" id="mask">--</td></tr>
    <tr><td class="k">MAC 地址</td><td class="v" id="mac">--</td></tr>
    <tr><td class="k">信号强度</td><td class="v" id="rssi">--</td></tr>
    <tr><td class="k">信道</td><td class="v" id="ch">--</td></tr>
  </table>
</div>

<div class="card">
  <h2>系统信息</h2>
  <table>
    <tr><td class="k">运行时间</td><td class="v" id="uptime">--</td></tr>
    <tr><td class="k">可用内存</td><td class="v" id="heap">--</td></tr>
    <tr><td class="k">PSRAM 可用</td><td class="v" id="psram">--</td></tr>
    <tr><td class="k">Flash</td><td class="v" id="flash">--</td></tr>
    <tr><td class="k">芯片</td><td class="v" id="chip">--</td></tr>
    <tr><td class="k">CPU 频率</td><td class="v" id="cpu">--</td></tr>
    <tr><td class="k">核心温度</td><td class="v" id="temp">--</td></tr>
    <tr><td class="k">SDK 版本</td><td class="v" id="sdk">--</td></tr>
  </table>
</div>

<footer>页面每 2 秒自动刷新</footer>
</div>

<script>
function q(id,v){document.getElementById(id).textContent=v}
function fmtKB(kb){return kb>=1024?(kb/1024).toFixed(2)+' MB':kb+' KB'}
async function refresh(){
  try{
    const d=await(await fetch('/api/status')).json();
    const box=document.getElementById('stateBox');
    if(d.connected){
      q('stateText','在线');box.className='badge on';
    }else if(d.mode==='AP'){
      q('stateText','AP 直连模式');box.className='badge ap';
    }else{
      q('stateText','离线');box.className='badge off';
    }
    q('mode',d.mode==='STA'?'STA(已连路由器)':'AP(直连模式)');
    q('host',d.host);q('ssid',d.ssid);q('ip',d.ip);q('gw',d.gw);q('mask',d.mask);q('mac',d.mac);
    if(d.mode==='STA'){
      const qs=d.rssi>=-50?'极好':d.rssi>=-60?'良好':d.rssi>=-70?'一般':'较弱';
      q('rssi',d.rssi+' dBm ('+qs+')');q('ch',d.ch);
    }else{
      q('rssi','—');q('ch','—');
    }
    q('uptime',d.uptime);
    q('heap',fmtKB(d.heapFree)+' / '+fmtKB(d.heapSize));
    q('psram',d.psramSize>0?fmtKB(d.psramFree)+' / '+fmtKB(d.psramSize):'无');
    q('flash',d.flashMB+' MB @ '+d.flashMHz+' MHz');
    q('chip',d.chip);
    q('cpu',d.cpuMHz+' MHz');q('temp',d.tempC+' ℃');q('sdk',d.sdk);
  }catch(e){
    q('stateText','设备无响应');
    document.getElementById('stateBox').className='badge off';
  }
}
refresh();
setInterval(refresh,2000);
</script>
</body>
</html>
)rawliteral";

/* ---------------- 工具 ---------------- */

// 转义 JSON 字符串中的特殊字符
static String jsonEsc(const String &s) {
  String r;
  r.reserve(s.length());
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if ((unsigned char)c < 0x20) r += ' ';
    else r += c;
  }
  return r;
}

/* ---------------- HTTP 接口 ---------------- */

// /api/status 返回设备状态 JSON
static void handleStatus() {
  DeviceStatus s = collectStatus();

  String j;
  j.reserve(640);
  j += "{\"mode\":\"" + s.mode + "\"";
  j += ",\"connected\":" + String(s.connected ? "true" : "false");
  j += ",\"ssid\":\"" + jsonEsc(s.ssid) + "\"";
  j += ",\"ip\":\"" + s.ip + "\"";
  j += ",\"gw\":\"" + s.gw + "\"";
  j += ",\"mask\":\"" + s.mask + "\"";
  j += ",\"mac\":\"" + s.mac + "\"";
  j += ",\"rssi\":" + String(s.rssi);
  j += ",\"ch\":" + String(s.channel);
  j += ",\"host\":\"" + jsonEsc(s.hostname) + "\"";
  j += ",\"uptime\":\"" + s.uptime + "\"";
  j += ",\"heapFree\":" + String(s.heapFree);
  j += ",\"heapSize\":" + String(s.heapSize);
  j += ",\"psramFree\":" + String(s.psramFree);
  j += ",\"psramSize\":" + String(s.psramSize);
  j += ",\"flashMB\":" + String(s.flashMB);
  j += ",\"flashMHz\":" + String(s.flashMHz);
  j += ",\"chip\":\"" + jsonEsc(s.chip) + "\"";
  j += ",\"cpuMHz\":" + String(s.cpuMHz);
  j += ",\"tempC\":\"" + s.tempC + "\"";
  j += ",\"sdk\":\"" + jsonEsc(s.sdk) + "\"}";
  server.send(200, "application/json", j);
}

/* ---------------- 模块接口 ---------------- */

void begin() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  // 后续新增功能在此注册路由即可,例如 /api/led、/api/reboot 等
  server.onNotFound([]() {
    server.send(404, "text/plain; charset=utf-8", "404: Not Found");
  });
  server.begin();

  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", WEB_PORT);
  }

  IPAddress ip = (wifi_service::mode() == NetMode::STA)
                     ? WiFi.localIP()
                     : WiFi.softAPIP();
  Serial.printf("[Web] 网页服务已启动: http://%s/\n", ip.toString().c_str());
  Serial.println("[Web] 部分设备也可用: http://" MDNS_HOST ".local/");
}

void loop() { server.handleClient(); }

}  // namespace web_ui
