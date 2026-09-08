#include "web_ui.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "auto_ctrl.h"
#include "config.h"
#include "device_status.h"
#include "port_service.h"
#include "wifi_service.h"

namespace web_ui {

static WebServer server(WEB_PORT);

/* ---------------- 首页 HTML(存于 Flash,节省 RAM) ---------------- */
// 布局:设备状态卡 + 传感数据卡(进度条) + 执行器控制卡;原收发窗口收进二级日志页
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>智慧农场控制台</title>
<style>
*{box-sizing:border-box}
body{font-family:"Microsoft YaHei",system-ui,sans-serif;background:#f2f3f5;color:#333;margin:0;padding:12px}
.wrap{max-width:760px;margin:0 auto}
h1{font-size:18px;text-align:center;margin:6px 0 12px}
h2{font-size:14px;color:#666;margin:0 0 8px;font-weight:600}
.card{background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.08);padding:12px 14px;margin-bottom:12px}
.cardhead{display:flex;align-items:center;justify-content:space-between}
.badge{display:flex;align-items:center;justify-content:space-between;padding:2px 0 8px;font-size:13px;font-weight:600}
.dot{width:9px;height:9px;border-radius:50%;background:#ccc;display:inline-block;margin-right:6px}
.on .dot{background:#2ecc71}
.ap .dot{background:#f39c12}
.off .dot{background:#e74c3c}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:2px 16px;font-size:12px}
.item{display:flex;justify-content:space-between;gap:8px;padding:3px 0;border-bottom:1px dashed #eee;min-width:0}
.item .k{color:#999;flex-shrink:0}
.item .v{font-weight:600;text-align:right;word-break:break-all}
/* 传感数据卡:3 列 */
.sensors{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}
.scard{background:#f7f8fa;border:1px solid #eee;border-radius:8px;padding:10px;text-align:center}
.sname{font-size:12px;color:#888;margin-bottom:4px}
.sval{font-size:20px;font-weight:700;color:#333;margin-bottom:6px}
.sval small{font-size:11px;font-weight:400;color:#999}
.bar{height:6px;background:#e8e9eb;border-radius:3px;overflow:hidden}
.barfill{height:100%;width:0;background:#3498db;border-radius:3px;transition:width .4s}
.barfill.warn{background:#f39c12}
.scard.ok .barfill{background:#2ecc71}
/* 执行器控制 */
.actrow{display:flex;align-items:center;gap:8px;padding:8px 0;border-bottom:1px dashed #eee;font-size:13px}
.actrow:last-child{border-bottom:0}
.actrow .lbl{width:56px;color:#666;flex-shrink:0}
.actrow button{border:0;border-radius:6px;background:#3498db;color:#fff;padding:7px 14px;font-size:13px;cursor:pointer}
.actrow button:active{background:#217db9}
.actrow button.green{background:#2ecc71}
.actrow button.red{background:#e74c3c}
.actrow button.gray{background:#95a5a6}
.actrow input[type=range]{flex:1;min-width:0}
.servoval{width:40px;text-align:center;font-weight:700;font-size:14px}
.astat{margin-left:auto;font-size:12px;font-weight:700;color:#999}
.astat.on{color:#27ae60}
.astat.act{color:#e67e22}
/* 自动控制卡 */
.actrow input[type=number]{width:76px;border:1px solid #ddd;border-radius:6px;padding:6px 8px;font-size:13px}
.actrow .unit{font-size:12px;color:#999}
.rule{display:flex;align-items:center;gap:8px;padding:5px 0;font-size:12px;color:#666}
.rule .rdot{width:8px;height:8px;border-radius:50%;background:#ccc;flex-shrink:0}
.rule.on .rdot{background:#e74c3c}
.rule.on{color:#c0392b;font-weight:600}
/* 日志二级页(全屏覆盖) */
.logpage{position:fixed;inset:0;background:#fff;z-index:10;overflow-y:auto;display:none;padding:12px}
.logpage.show{display:block}
.loghead{display:flex;align-items:center;justify-content:space-between;max-width:760px;margin:0 auto 10px}
.loghead button{border:0;border-radius:6px;background:#e74c3c;color:#fff;padding:7px 16px;font-size:13px;cursor:pointer}
.ports{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;max-width:760px;margin:0 auto}
.pcard{background:#fff;border:1px solid #eee;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.08);padding:10px 12px;display:flex;flex-direction:column}
.phead{display:flex;align-items:center;gap:6px;font-size:13px;padding-bottom:6px}
.pdot{width:8px;height:8px;border-radius:50%;background:#ccc;flex-shrink:0}
.pdot.live{background:#2ecc71}
.pdot.link{background:#f39c12}
.pinfo{margin-left:auto;color:#888;font-size:11px;font-weight:600}
.pinfo.on{color:#27ae60}
.plog{flex:1;min-height:120px;overflow-y:auto;background:#f7f8fa;border:1px solid #eee;border-radius:6px;padding:6px;margin:0;font:11px/1.5 Consolas,monospace;white-space:pre-wrap;word-break:break-all;color:#555}
.psend{display:flex;gap:6px;margin-top:8px}
.psend input{flex:1;min-width:0;border:1px solid #ddd;border-radius:6px;padding:6px 8px;font-size:12px}
.psend button{border:0;border-radius:6px;background:#3498db;color:#fff;padding:6px 14px;font-size:12px;cursor:pointer}
.pstat{font-size:11px;color:#aaa;margin-top:6px;text-align:right}
#logBtn{display:block;width:100%;border:0;border-radius:10px;background:#34495e;color:#fff;padding:10px;font-size:14px;cursor:pointer;margin-bottom:4px}
footer{text-align:center;color:#aaa;font-size:11px;margin:4px 0}
</style>
</head>
<body>
<div class="wrap">
<h1>智慧农场控制台</h1>

<!-- 设备状态卡 -->
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
    <div class="item"><span class="k">主机名</span><span class="v" id="hostname">--</span></div>
    <div class="item"><span class="k">运行时间</span><span class="v" id="uptime">--</span></div>
  </div>
</div>

<!-- 传感数据卡 -->
<div class="card">
  <div class="cardhead"><h2>传感数据（F8266-1）</h2><span id="dataState" style="font-size:11px;color:#999">等待从机…</span></div>
  <div class="sensors">
    <div class="scard"><div class="sname">温度</div><div class="sval" id="s_t">--<small>℃</small></div><div class="bar"><div class="barfill" id="b_t"></div></div></div>
    <div class="scard"><div class="sname">湿度</div><div class="sval" id="s_h">--<small>%</small></div><div class="bar"><div class="barfill" id="b_h"></div></div></div>
    <div class="scard"><div class="sname">光照</div><div class="sval" id="s_lux">--<small>lx</small></div><div class="bar"><div class="barfill" id="b_lux"></div></div></div>
    <div class="scard"><div class="sname">土壤</div><div class="sval" id="s_soil">--</div><div class="bar"><div class="barfill" id="b_soil"></div></div></div>
    <div class="scard"><div class="sname">CO₂</div><div class="sval" id="s_co2">--<small>ppm</small></div><div class="bar"><div class="barfill" id="b_co2"></div></div></div>
    <div class="scard"><div class="sname">TVOC</div><div class="sval" id="s_tvoc">--<small>ppb</small></div><div class="bar"><div class="barfill" id="b_tvoc"></div></div></div>
  </div>
</div>

<!-- 执行器控制 -->
<div class="card">
  <h2>执行器控制</h2>
  <div class="actrow"><span class="lbl">水泵</span><button class="green" onclick="act('PUMP ON')">开</button><button class="gray" onclick="act('PUMP OFF')">关</button><span class="astat" id="st_pump">--</span></div>
  <div class="actrow"><span class="lbl">风扇</span><button class="green" onclick="act('FAN FWD')">正转</button><button class="red" onclick="act('FAN REV')">反转</button><button class="gray" onclick="act('FAN STOP')">停</button><span class="astat" id="st_fan">--</span></div>
  <div class="actrow"><span class="lbl">舵机</span><input type="range" id="servo" min="0" max="180" value="0" oninput="document.getElementById('servoVal').textContent=this.value" onchange="act('SERVO '+this.value)"><span class="servoval" id="servoVal">0</span>°<span class="astat" id="st_servo">--</span></div>
</div>

<!-- 自动控制 -->
<div class="card">
  <div class="cardhead"><h2>自动控制</h2><button id="autoBtn" class="gray" onclick="toggleAuto()">手动</button></div>
  <div class="actrow"><span class="lbl">高温</span><input type="number" id="ac_t" step="0.5" min="0"><span class="unit">℃ 触发风扇正转</span></div>
  <div class="actrow"><span class="lbl">CO₂</span><input type="number" id="ac_co2" step="50" min="0"><span class="unit">ppm 触发风扇正转</span></div>
  <div class="actrow"><span class="lbl">高湿</span><input type="number" id="ac_h" step="1" min="0"><span class="unit">% 触发风扇反转排湿</span></div>
  <div class="actrow"><button id="saveBtn" class="green" onclick="saveAuto()">保存阈值</button></div>
  <div id="rules"></div>
</div>

<button id="logBtn" onclick="document.getElementById('logPage').classList.add('show')">📋 通信日志</button>
<footer>状态 2 秒 / 数据 1.5 秒自动刷新</footer>
</div>

<!-- 日志二级页(原 2×3 收发窗口) -->
<div class="logpage" id="logPage">
  <div class="loghead"><h2>从机通信日志</h2><button onclick="document.getElementById('logPage').classList.remove('show')">✕ 关闭</button></div>
  <div class="ports" id="ports"></div>
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
    q('hostname',d.host);q('uptime',d.uptime);
  }catch(e){
    q('stateText','设备无响应');
    document.getElementById('stateBox').className='badge off';
  }
}

/* ---------- 传感数据(取端口 0 = F8266-1) ---------- */
function setSensor(key,val,unit,pct,warn){
  // 此处需渲染 <small> 单位标签,须用 innerHTML 而非 textContent(q)
  document.getElementById('s_'+key).innerHTML = val===null?'--':val+'<small>'+unit+'</small>';
  const bar=document.getElementById('b_'+key);
  bar.style.width=(pct*100)+'%';
  bar.className='barfill'+(warn?' warn':'');
}
function refreshSensors(){
  if(!portsData||!portsData[0])return;
  const d=portsData[0];
  const ds=document.getElementById('dataState');
  if(!d.hasData){ds.textContent='等待数据…';return}
  ds.textContent=d.online?d.slave+' 在线':'从机已离线';
  setSensor('t',d.t,'℃',d.t===null?0:Math.min(d.t/50,1),d.t!==null&&d.t>35);
  setSensor('h',d.h,'%',d.h===null?0:Math.min(d.h/100,1),false);
  setSensor('lux',d.lux,'lx',d.lux===null?0:Math.min(d.lux/2000,1),false);
  if(d.soil===null){q('s_soil','--');document.getElementById('b_soil').style.width='0'}
  else if(d.soil===1){q('s_soil','湿润');document.getElementById('b_soil').style.width='100%';document.getElementById('b_soil').className='barfill'}
  else{q('s_soil','干燥');document.getElementById('b_soil').style.width='20%';document.getElementById('b_soil').className='barfill warn'}
  setSensor('co2',d.co2,'ppm',d.co2===null?0:Math.min((d.co2-400)/1600,1),d.co2!==null&&d.co2>1200);
  setSensor('tvoc',d.tvoc,'ppb',d.tvoc===null?0:Math.min(d.tvoc/1000,1),d.tvoc!==null&&d.tvoc>500);
  // 执行器实时状态(从机回报)
  const sp=document.getElementById('st_pump');
  sp.textContent=d.pump===1?'开':'关';sp.className='astat'+(d.pump===1?' on':'');
  const sf=document.getElementById('st_fan');
  const fn=d.fan===1?'正转':(d.fan===2?'反转':'停');
  sf.textContent=fn;sf.className='astat'+(d.fan>0?' act':'');
  // 舵机=大棚:0°关 / 180°开(角度同步;滑条拖动中不强制覆盖)
  const sv=document.getElementById('servoVal'),ss=document.getElementById('st_servo');
  const gh=d.servo===0?'关棚':(d.servo===180?'开棚':('角度'+d.servo));
  ss.textContent=gh;ss.className='astat'+(d.servo===180?' act':'');
  if(document.activeElement!==document.getElementById('servo')){
    sv.textContent=d.servo;
    document.getElementById('servo').value=d.servo;
  }
}

/* ---------- 执行器控制(发命令到端口 8000) ---------- */
async function act(cmd){
  try{
    const body=new URLSearchParams({port:'8000',text:cmd});
    const r=await(await fetch('/api/send',{method:'POST',body:body})).json();
    if(!r.ok)alert('控制失败：'+(r.error||'从机离线'));
  }catch(e){alert('控制失败：网络错误')}
}

/* ---------- 从机通信端口(日志二级页内) ---------- */
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
    refreshSensors();
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

/* ---------- 自动控制 ---------- */
let autoInit=false;
function renderRules(rules){
  document.getElementById('rules').innerHTML=rules.map(r=>
    '<div class="rule'+(r.active?' on':'')+'"><span class="rdot"></span>'+r.name+(r.active?' 已触发':' 未触发')+'</div>').join('');
}
async function refreshAuto(){
  try{
    const d=await(await fetch('/api/auto')).json();
    // 仅首次加载填充输入框;之后不再覆盖,避免冲掉用户正在编辑的阈值
    if(!autoInit){
      document.getElementById('ac_t').value=d.tHigh;
      document.getElementById('ac_co2').value=d.co2High;
      document.getElementById('ac_h').value=d.hHigh;
      autoInit=true;
    }
    const b=document.getElementById('autoBtn');
    b.textContent=d.enabled?'自动':'手动';
    b.className=d.enabled?'green':'gray';
    renderRules(d.rules);
  }catch(e){}
}
async function saveAuto(){
  const t=document.getElementById('ac_t').value;
  const c=document.getElementById('ac_co2').value;
  const h=document.getElementById('ac_h').value;
  const en=document.getElementById('autoBtn').textContent==='自动';
  try{
    const body=new URLSearchParams({enabled:String(en),tHigh:t,co2High:c,hHigh:h});
    const r=await(await fetch('/api/auto',{method:'POST',body:body})).json();
    if(r.ok){
      const sb=document.getElementById('saveBtn');
      sb.textContent='已保存 ✓'; sb.disabled=true;
      setTimeout(()=>{sb.textContent='保存阈值';sb.disabled=false},1500);
      refreshAuto();
    } else alert('保存失败：'+(r.error||''));
  }catch(e){alert('保存失败：网络错误')}
}
async function toggleAuto(){
  const b=document.getElementById('autoBtn');
  const nowAuto=b.textContent==='自动';
  b.textContent=nowAuto?'手动':'自动';
  b.className=nowAuto?'gray':'green';
  await saveAuto();
}
refreshAuto();setInterval(refreshAuto,2000);
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

// 数字或 null(传感字段 NaN 表示无数据)
static String numOrNull(float v, int dec = 1) {
  if (isnan(v)) return "null";
  return String(v, dec);
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
    // 传感数据(无数据字段为 null)
    j += ",\"hasData\":" + String(s.hasData ? "true" : "false");
    j += ",\"t\":" + numOrNull(s.t);
    j += ",\"h\":" + numOrNull(s.h);
    j += ",\"lux\":" + (isnan(s.lux) ? String("null") : String((int)s.lux));
    j += ",\"soil\":" + (s.soil < 0 ? String("null") : String(s.soil));
    j += ",\"co2\":" + (isnan(s.co2) ? String("null") : String((int)s.co2));
    j += ",\"tvoc\":" + (isnan(s.tvoc) ? String("null") : String((int)s.tvoc));
    // 执行器实时状态
    j += ",\"pump\":" + String(s.pump);
    j += ",\"fan\":" + String(s.fan);
    j += ",\"servo\":" + String(s.servo);
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

// /api/auto GET:返回自动控制配置 + 各规则触发状态
static void handleGetAuto() {
  auto_ctrl::Config c = auto_ctrl::getConfig();
  String j;
  j.reserve(256);
  j += "{\"enabled\":" + String(c.enabled ? "true" : "false");
  j += ",\"tHigh\":" + String(c.tHigh);
  j += ",\"co2High\":" + String(c.co2High);
  j += ",\"hHigh\":" + String(c.hHigh);
  j += ",\"fanDir\":" + String(auto_ctrl::fanDir());   // 0=停 1=正转 2=反转
  j += ",\"pumpActive\":" + String(auto_ctrl::pumpActive() ? "true" : "false");
  j += ",\"servoActive\":" + String(auto_ctrl::servoActive() ? "true" : "false");
  j += ",\"rules\":[";
  for (uint8_t i = 0; i < auto_ctrl::ruleCount(); i++) {
    if (i) j += ",";
    auto_ctrl::RuleState r = auto_ctrl::ruleState(i);
    j += "{\"name\":\"" + jsonEsc(r.name) + "\"";
    j += ",\"active\":" + String(r.active ? "true" : "false") + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

// /api/auto POST:保存配置(enabled/tHigh/co2High/hHigh)并写 NVS
static void handleSetAuto() {
  if (!server.hasArg("tHigh") || !server.hasArg("co2High") || !server.hasArg("hHigh")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"缺少阈值参数\"}");
    return;
  }
  auto_ctrl::Config c;
  c.enabled = server.arg("enabled") == "true";
  c.tHigh   = server.arg("tHigh").toFloat();
  c.co2High = server.arg("co2High").toFloat();
  c.hHigh   = server.arg("hHigh").toFloat();
  auto_ctrl::setConfig(c);
  server.send(200, "application/json", "{\"ok\":true}");
}

/* ---------------- 模块接口 ---------------- */

void begin() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/ports", HTTP_GET, handlePorts);
  server.on("/api/send", HTTP_POST, handleSend);
  server.on("/api/auto", HTTP_GET, handleGetAuto);
  server.on("/api/auto", HTTP_POST, handleSetAuto);
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
