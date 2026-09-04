#include "web_ui.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "device_status.h"
#include "port_service.h"
#include "wifi_service.h"

namespace web_ui {

static WebServer server(WEB_PORT);

/* ---------------- 首页 HTML(存于 Flash,节省 RAM) ---------------- */
// 布局:上方为精简设备状态卡,下方为 2×3 六个从机通信端口卡(222 布局)
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 主机控制台</title>
<style>
*{box-sizing:border-box}
body{font-family:"Microsoft YaHei",system-ui,sans-serif;background:#f2f3f5;color:#333;margin:0;padding:12px}
.wrap{max-width:760px;margin:0 auto}
h1{font-size:18px;text-align:center;margin:6px 0 12px}
h2{font-size:14px;color:#666;margin:2px 0 8px;font-weight:600}
.card{background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.08);padding:12px 14px;margin-bottom:12px}
.badge{display:flex;align-items:center;justify-content:space-between;padding:2px 0 8px;font-size:13px;font-weight:600}
.dot{width:9px;height:9px;border-radius:50%;background:#ccc;display:inline-block;margin-right:6px}
.on .dot{background:#2ecc71}
.ap .dot{background:#f39c12}
.off .dot{background:#e74c3c}
/* 设备状态精简卡:两列键值网格 */
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:2px 16px;font-size:12px}
.item{display:flex;justify-content:space-between;gap:8px;padding:3px 0;border-bottom:1px dashed #eee;min-width:0}
.item .k{color:#999;flex-shrink:0}
.item .v{font-weight:600;text-align:right;word-break:break-all}
/* 从机端口卡:2 列 x 3 行 */
.ports{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.pcard{background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.08);padding:10px 12px;display:flex;flex-direction:column}
.phead{display:flex;align-items:center;gap:6px;font-size:13px;padding-bottom:6px}
.pdot{width:8px;height:8px;border-radius:50%;background:#ccc;flex-shrink:0}
.pdot.live{background:#2ecc71}
.pdot.link{background:#f39c12}
.pinfo{margin-left:auto;color:#888;font-size:11px;font-weight:600}
.pinfo.on{color:#27ae60}
.plog{flex:1;min-height:90px;max-height:130px;overflow-y:auto;background:#f7f8fa;border:1px solid #eee;border-radius:6px;padding:6px;margin:0;font:11px/1.5 Consolas,monospace;white-space:pre-wrap;word-break:break-all;color:#555}
.psend{display:flex;gap:6px;margin-top:8px}
.psend input{flex:1;min-width:0;border:1px solid #ddd;border-radius:6px;padding:6px 8px;font-size:12px}
.psend button{border:0;border-radius:6px;background:#3498db;color:#fff;padding:6px 14px;font-size:12px;cursor:pointer}
.psend button:active{background:#217db9}
.pstat{font-size:11px;color:#aaa;margin-top:6px;text-align:right}
footer{text-align:center;color:#aaa;font-size:11px;margin:4px 0}
</style>
</head>
<body>
<div class="wrap">
<h1>ESP32-S3 主机控制台</h1>

<!-- 设备状态(原状态页内容精简收录于此卡) -->
<div class="card">
  <div class="badge" id="stateBox">
    <span><span class="dot"></span><span id="stateText">连接中…</span></span>
    <span id="host" style="color:#888"></span>
  </div>
  <div class="grid">
    <div class="item"><span class="k">模式</span><span class="v" id="mode">--</span></div>
    <div class="item"><span class="k">SSID</span><span class="v" id="ssid">--</span></div>
    <div class="item"><span class="k">IP</span><span class="v" id="ip">--</span></div>
    <div class="item"><span class="k">信号</span><span class="v" id="rssi">--</span></div>
    <div class="item"><span class="k">网关</span><span class="v" id="gw">--</span></div>
    <div class="item"><span class="k">信道</span><span class="v" id="ch">--</span></div>
    <div class="item"><span class="k">掩码</span><span class="v" id="mask">--</span></div>
    <div class="item"><span class="k">MAC</span><span class="v" id="mac">--</span></div>
    <div class="item"><span class="k">主机名</span><span class="v" id="hostname">--</span></div>
    <div class="item"><span class="k">运行时间</span><span class="v" id="uptime">--</span></div>
    <div class="item"><span class="k">内存</span><span class="v" id="heap">--</span></div>
    <div class="item"><span class="k">PSRAM</span><span class="v" id="psram">--</span></div>
    <div class="item"><span class="k">Flash</span><span class="v" id="flash">--</span></div>
    <div class="item"><span class="k">芯片</span><span class="v" id="chip">--</span></div>
    <div class="item"><span class="k">CPU</span><span class="v" id="cpu">--</span></div>
    <div class="item"><span class="k">温度</span><span class="v" id="temp">--</span></div>
    <div class="item"><span class="k">SDK</span><span class="v" id="sdk">--</span></div>
  </div>
</div>

<h2>从机通信端口</h2>
<div class="ports" id="ports"></div>

<footer>状态每 2 秒 / 端口每 1.5 秒自动刷新</footer>
</div>

<script>
function q(id,v){document.getElementById(id).textContent=v}
function fmtKB(kb){return kb>=1024?(kb/1024).toFixed(2)+' MB':kb+' KB'}
let portsData=null;

/* ---------- 设备状态 ---------- */
async function refreshStatus(){
  try{
    const d=await(await fetch('/api/status')).json();
    const box=document.getElementById('stateBox');
    if(d.connected){q('stateText','在线');box.className='badge on'}
    else if(d.mode==='AP'){q('stateText','AP 直连模式');box.className='badge ap'}
    else{q('stateText','离线');box.className='badge off'}
    q('host',d.host);
    q('mode',d.mode==='STA'?'STA(路由器)':'AP(直连)');
    q('ssid',d.ssid);q('ip',d.ip);
    q('rssi',d.mode==='STA'?d.rssi+' dBm':'—');
    q('gw',d.gw);q('ch',d.mode==='STA'?d.ch:'—');
    q('mask',d.mask);q('mac',d.mac);q('hostname',d.host);q('uptime',d.uptime);
    q('heap',fmtKB(d.heapFree)+' / '+fmtKB(d.heapSize));
    q('psram',d.psramSize>0?fmtKB(d.psramFree)+' / '+fmtKB(d.psramSize):'无');
    q('flash',d.flashMB+'MB @ '+d.flashMHz+'MHz');
    q('chip',d.chip);q('cpu',d.cpuMHz+' MHz');q('temp',d.tempC+' ℃');q('sdk',d.sdk);
  }catch(e){
    q('stateText','设备无响应');
    document.getElementById('stateBox').className='badge off';
  }
}

/* ---------- 从机通信端口 ---------- */
function buildCards(list){
  document.getElementById('ports').innerHTML=list.map((d,i)=>
    '<div class="pcard">'+
      '<div class="phead"><span class="pdot" id="pd'+i+'"></span><b>端口 '+d.port+'</b>'+
      '<span class="pinfo" id="pn'+i+'">无从机</span></div>'+
      '<pre class="plog" id="pl'+i+'"></pre>'+
      '<div class="psend"><input id="pi'+i+'" placeholder="输入要发送的内容">'+
      '<button id="pb'+i+'">发送</button></div>'+
      '<div class="pstat" id="ps'+i+'">收 0 / 发 0</div>'+
    '</div>').join('');
  list.forEach((d,i)=>{
    document.getElementById('pb'+i).onclick=()=>sendMsg(i);
    document.getElementById('pi'+i).onkeydown=e=>{if(e.key==='Enter')sendMsg(i)};
  });
}
async function refreshPorts(){
  try{
    const a=await(await fetch('/api/ports')).json();
    if(!portsData){portsData=a;buildCards(a)}
    portsData=a;
    a.forEach((d,i)=>{
      const dot=document.getElementById('pd'+i),info=document.getElementById('pn'+i);
      if(d.online){dot.className='pdot live';info.className='pinfo on';info.textContent=d.slave+' 已上线'}
      else if(d.clients>0){dot.className='pdot link';info.className='pinfo';info.textContent='已连接·未上线'}
      else{dot.className='pdot';info.className='pinfo';info.textContent='无从机'}
      const log=document.getElementById('pl'+i);
      log.textContent=d.log.join('\n')||'等待从机连接…';
      log.scrollTop=log.scrollHeight;
      q('ps'+i,'收 '+d.rx+' / 发 '+d.tx+(d.clients>0?' · 连接 '+d.clients:''));
    });
  }catch(e){}
}
async function sendMsg(i){
  const inp=document.getElementById('pi'+i),text=inp.value.trim();
  if(!text||!portsData)return;
  try{
    const body=new URLSearchParams({port:String(portsData[i].port),text:text});
    const r=await(await fetch('/api/send',{method:'POST',body:body})).json();
    if(r.ok){inp.value='';refreshPorts()}
    else alert('发送失败：'+(r.error||'未知错误'));
  }catch(e){alert('发送失败：网络错误')}
}
refreshStatus();setInterval(refreshStatus,2000);
refreshPorts();setInterval(refreshPorts,1500);
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

// /api/status 设备状态 JSON
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

// /api/ports 六个从机通信端口状态 JSON
static void handlePorts() {
  String j;
  j.reserve(1600);
  j += "[";
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    PortSnapshot s = port_service::snapshot(i);
    if (i) j += ",";
    j += "{\"port\":" + String(s.port);
    j += ",\"clients\":" + String(s.clients);
    j += ",\"online\":" + String(s.online ? "true" : "false");
    j += ",\"slave\":\"" + jsonEsc(s.slave) + "\"";
    j += ",\"rx\":" + String(s.rxCount);
    j += ",\"tx\":" + String(s.txCount);
    j += ",\"log\":[";
    for (uint8_t k = 0; k < s.logLen; k++) {
      if (k) j += ",";
      j += "\"" + jsonEsc(s.log[k]) + "\"";
    }
    j += "]}";
  }
  j += "]";
  server.send(200, "application/json", j);
}

// /api/send 向指定端口发送报文(POST: port=8000&text=hello)
static void handleSend() {
  if (!server.hasArg("port") || !server.hasArg("text") || server.arg("text").isEmpty()) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"缺少 port/text 参数\"}");
    return;
  }
  long port = server.arg("port").toInt();
  if (port < PORT_BASE || port >= PORT_BASE + PORT_COUNT) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"端口不在监听范围内\"}");
    return;
  }
  bool ok = port_service::send(port - PORT_BASE, server.arg("text"));
  server.send(200, "application/json",
              String("{\"ok\":") + (ok ? "true" : "false") +
                  ",\"error\":\"" + (ok ? "" : "该端口无已连接从机") + "\"}");
}

/* ---------------- 模块接口 ---------------- */

void begin() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/ports", HTTP_GET, handlePorts);
  server.on("/api/send", HTTP_POST, handleSend);
  // 后续新增功能在此注册路由即可
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
