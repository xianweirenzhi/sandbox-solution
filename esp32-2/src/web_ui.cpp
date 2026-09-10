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
// 布局 v2(响应式,自适应手机竖屏 / PC 宽屏):
//   顶栏(在线状态/主机名/IP/运行时间/日志入口)
//   从机总览(六从机卡片:6 指标迷你进度条 + 执行器状态章,点击跳详情)
//   从机详情(可切从机:传感大卡+趋势折线 + 执行器控制 + 每从机独立自动控制)
//   通信日志二级页(六端口收发,PC 三列/手机两列)
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>智慧农场控制中心</title>
<style>
:root{
  --bg:#f3f5f8;--card:#ffffff;--ink:#1b2733;--mut:#8695a4;--line:#e7ecf1;
  --acc:#2f7df6;--ok:#21b573;--warn:#f39c12;--bad:#e5533c;--chip:#eef2f7;
  --bar:#e4e9ef;--sh:0 1px 2px rgba(16,24,40,.05),0 4px 16px rgba(16,24,40,.07)
}
@media(prefers-color-scheme:dark){:root{
  --bg:#0f151c;--card:#18212b;--ink:#e7edf3;--mut:#7f8fa0;--line:#253141;
  --acc:#4a8dff;--ok:#2fc483;--warn:#f5a623;--bad:#ff6b52;--chip:#202c3a;
  --bar:#243244;--sh:0 1px 2px rgba(0,0,0,.35),0 4px 16px rgba(0,0,0,.35)
}}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.5 system-ui,-apple-system,"PingFang SC","Microsoft YaHei",sans-serif}
.top{position:sticky;top:0;z-index:9;background:var(--card);box-shadow:var(--sh)}
.twrap{max-width:1160px;margin:0 auto;padding:10px 16px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.ttl{display:flex;align-items:center;gap:8px;font-size:17px;font-weight:700}
.sub{font-size:12px;color:var(--mut);font-weight:400}
.dot{width:9px;height:9px;border-radius:50%;background:#c3ccd6;flex-shrink:0;display:inline-block}
.dot.on{background:var(--ok)}.dot.ap{background:var(--warn)}.dot.off{background:var(--bad)}
.tmeta{margin-left:auto;display:flex;align-items:center;gap:10px;font-size:12px;color:var(--mut)}
.pill{background:var(--chip);border-radius:999px;padding:4px 12px;font-weight:600;color:var(--ink);white-space:nowrap}
.wrap{max-width:1160px;margin:0 auto;padding:14px 16px 26px}
.sechead{display:flex;align-items:center;gap:10px;margin:4px 2px 10px}
h2{font-size:15px;margin:0}
h3{font-size:14px;margin:0}
.mut{color:var(--mut);font-size:12px}
.card{background:var(--card);border-radius:14px;box-shadow:var(--sh);padding:14px 16px;margin-bottom:14px}
.cardhead{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:10px}
.secgap{margin-top:20px}
.ovgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:12px}
.scard{background:var(--card);border-radius:14px;box-shadow:var(--sh);padding:12px 14px;cursor:pointer;transition:transform .15s}
.scard:hover{transform:translateY(-2px)}
.scard.off{opacity:.62}
.schead{display:flex;align-items:center;gap:7px;margin-bottom:8px}
.schead b{font-size:14px}
.schead .port{font-size:11px;color:var(--mut)}
.stchip{margin-left:auto;font-size:11px;font-weight:700;color:var(--mut)}
.stchip.on{color:var(--ok)}
.mgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:7px 12px}
.mi{min-width:0}
.mi .k{font-size:11px;color:var(--mut)}
.mi .v{font-size:14px;font-weight:700;white-space:nowrap}
.mi .v small{font-size:10px;font-weight:400;color:var(--mut);margin-left:1px}
.bar{height:4px;background:var(--bar);border-radius:2px;overflow:hidden;margin-top:3px}
.bar i{display:block;height:100%;width:0;background:var(--acc);border-radius:2px;transition:width .4s}
.bar i.warn{background:var(--warn)}
.bar i.ok{background:var(--ok)}
.achips{display:flex;gap:6px;flex-wrap:wrap;margin-top:9px}
.achip{font-size:11px;background:var(--chip);color:var(--mut);border-radius:999px;padding:2px 9px;font-weight:600}
.achip.act{background:transparent;color:var(--warn);box-shadow:inset 0 0 0 1px var(--warn)}
.achip.on{background:transparent;color:var(--ok);box-shadow:inset 0 0 0 1px var(--ok)}
.afoot{margin-top:8px;font-size:11px;color:var(--mut)}
.tabs{display:flex;gap:6px;overflow-x:auto;padding-bottom:2px;scrollbar-width:none;margin-left:auto}
.tabs::-webkit-scrollbar{display:none}
.tab{border:0;background:var(--card);color:var(--ink);border-radius:999px;padding:6px 13px;font-size:12px;font-weight:600;cursor:pointer;display:flex;align-items:center;gap:6px;box-shadow:var(--sh);flex-shrink:0}
.tab.on{background:var(--acc);color:#fff}
.tab .dot{width:7px;height:7px}
.tab.on .dot{background:rgba(255,255,255,.9)}
.sgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(148px,1fr));gap:10px}
.big{background:var(--chip);border-radius:12px;padding:12px;min-width:0}
.big .k{font-size:12px;color:var(--mut)}
.big .v{font-size:24px;font-weight:800;margin:2px 0 6px;white-space:nowrap}
.big .v small{font-size:12px;font-weight:400;color:var(--mut)}
.big canvas{width:100%;height:28px;display:block;margin-top:6px}
.twocol{display:grid;grid-template-columns:1fr;gap:14px}
@media(min-width:900px){.twocol{grid-template-columns:1fr 1fr}}
.actrow{display:flex;align-items:center;gap:8px;padding:9px 0;border-bottom:1px dashed var(--line);font-size:13px}
.actrow:last-of-type{border-bottom:0}
.lbl{width:44px;color:var(--mut);flex-shrink:0}
.btn{border:0;border-radius:8px;background:var(--acc);color:#fff;padding:7px 15px;font-size:13px;cursor:pointer;font-weight:600}
.btn:active{filter:brightness(.9)}
.btn.green{background:var(--ok)}
.btn.red{background:var(--bad)}
.btn.gray{background:var(--chip);color:var(--ink)}
.astat{margin-left:auto;font-size:12px;font-weight:700;color:var(--mut)}
.astat.on{color:var(--ok)}
.astat.act{color:var(--warn)}
.actrow input[type=number]{width:78px;border:1px solid var(--line);border-radius:8px;padding:6px 8px;font-size:13px;background:var(--card);color:var(--ink)}
.actrow input[type=range]{flex:1;min-width:0;accent-color:var(--acc)}
.unit{font-size:12px;color:var(--mut)}
.servoval{width:34px;text-align:center;font-weight:700}
.rule{display:flex;align-items:center;gap:8px;padding:4px 0;font-size:12px;color:var(--mut)}
.rule .rdot{width:8px;height:8px;border-radius:50%;background:#c3ccd6;flex-shrink:0}
.rule.on{color:var(--bad);font-weight:600}
.rule.on .rdot{background:var(--bad)}
.logpage{position:fixed;inset:0;background:var(--bg);z-index:20;overflow-y:auto;display:none;padding:14px 16px}
.logpage.show{display:block}
.loghead{display:flex;align-items:center;justify-content:space-between;max-width:1160px;margin:0 auto 12px}
.portgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:12px;max-width:1160px;margin:0 auto}
.pcard{background:var(--card);border-radius:14px;box-shadow:var(--sh);padding:12px 14px;display:flex;flex-direction:column}
.phead{display:flex;align-items:center;gap:7px;font-size:13px;padding-bottom:8px}
.pinfo{margin-left:auto;color:var(--mut);font-size:11px;font-weight:600}
.pinfo.on{color:var(--ok)}
.plog{flex:1;min-height:130px;max-height:190px;overflow-y:auto;background:var(--chip);border-radius:8px;padding:8px;margin:0;font:11px/1.6 Consolas,ui-monospace,monospace;white-space:pre-wrap;word-break:break-all;color:var(--mut)}
.psend{display:flex;gap:6px;margin-top:9px}
.psend input{flex:1;min-width:0;border:1px solid var(--line);border-radius:8px;padding:7px 9px;font-size:12px;background:var(--card);color:var(--ink)}
.psend .btn{padding:7px 14px;font-size:12px}
.pstat{font-size:11px;color:var(--mut);margin-top:7px;text-align:right}
footer{text-align:center;color:var(--mut);font-size:11px;margin:8px 0}
</style>
</head>
<body>
<header class="top"><div class="twrap">
  <div class="ttl"><span class="dot" id="stDot"></span>智慧农场控制中心<span class="sub" id="stSub"></span></div>
  <div class="tmeta"><span class="pill" id="stUp">--</span><button class="btn" onclick="document.getElementById('logPage').classList.add('show')">📋 日志</button></div>
</div></header>

<main class="wrap">
  <div class="sechead"><h2>从机总览</h2><span class="mut" id="ovSub"></span></div>
  <div class="ovgrid" id="ovGrid"></div>

  <div class="sechead secgap"><h2>从机详情</h2><div class="tabs" id="tabs"></div></div>
  <div class="card" id="detailCard">
    <div class="cardhead"><h3>传感数据 <span class="mut" id="dName"></span></h3><span class="mut" id="dState">等待连接…</span></div>
    <div class="sgrid" id="sGrid"></div>
  </div>
  <div class="twocol">
    <div class="card">
      <div class="cardhead"><h3>执行器控制</h3><span class="mut" id="actPort"></span></div>
      <div class="actrow"><span class="lbl">水泵</span><button class="btn green" onclick="act('PUMP ON')">开</button><button class="btn gray" onclick="act('PUMP OFF')">关</button><span class="astat" id="st_pump">--</span></div>
      <div class="actrow"><span class="lbl">风扇</span><button class="btn green" onclick="act('FAN FWD')">正转</button><button class="btn red" onclick="act('FAN REV')">反转</button><button class="btn gray" onclick="act('FAN STOP')">停</button><span class="astat" id="st_fan">--</span></div>
      <div class="actrow"><span class="lbl">舵机</span><input type="range" id="servo" min="0" max="180" value="0" oninput="document.getElementById('servoVal').textContent=this.value" onchange="act('SERVO '+this.value)"><span class="servoval" id="servoVal">0</span>°<span class="astat" id="st_servo">--</span></div>
    </div>
    <div class="card">
      <div class="cardhead"><h3>自动控制</h3><button id="autoBtn" class="btn gray" onclick="toggleAuto()">手动</button></div>
      <div class="actrow"><span class="lbl">高温</span><input type="number" id="ac_t" step="0.5" min="0"><span class="unit">℃ 触发风扇正转</span></div>
      <div class="actrow"><span class="lbl">CO₂</span><input type="number" id="ac_co2" step="50" min="0"><span class="unit">ppm 触发风扇正转</span></div>
      <div class="actrow"><span class="lbl">高湿</span><input type="number" id="ac_h" step="1" min="0"><span class="unit">% 触发风扇反转排湿</span></div>
      <div class="actrow"><span class="lbl">土壤干</span><input type="number" id="ac_sd" step="10" min="0" max="1023"><span class="unit">原始值 ≥此触发水泵浇水</span></div>
      <div class="actrow"><span class="lbl">土壤湿</span><input type="number" id="ac_sw" step="10" min="0" max="1023"><span class="unit">原始值 ≤此解除浇水（仅模拟量从机）</span></div>
      <div class="actrow"><button id="saveBtn" class="btn green" onclick="saveAuto()">保存阈值</button></div>
      <div id="rules"></div>
    </div>
  </div>
</main>

<div class="logpage" id="logPage">
  <div class="loghead"><h2>从机通信日志</h2><button class="btn red" onclick="document.getElementById('logPage').classList.remove('show')">✕ 关闭</button></div>
  <div class="portgrid" id="ports"></div>
</div>

<footer>总览与数据 1.5 秒 / 自动控制 2 秒自动刷新 · 数据经 8000~8005 六端口采集</footer>

<script>
function q(id,v){document.getElementById(id).textContent=v}
function el(id){return document.getElementById(id)}
let PD=null,sel=0;
let hist={},autoInited={};
/* 六指标定义:f=格式化 p=进度条占比 w=越界警示 soil 为 0/1 两态特殊处理 */
const METRICS=[
 {k:'t',n:'温度',u:'℃',f:v=>v.toFixed(1),p:v=>Math.min(v/50,1),w:v=>v>35},
 {k:'h',n:'湿度',u:'%',f:v=>v.toFixed(1),p:v=>Math.min(v/100,1),w:()=>false},
 {k:'lux',n:'光照',u:'lx',f:v=>Math.round(v),p:v=>Math.min(v/2000,1),w:()=>false},
 {k:'soil',n:'土壤',u:'',soil:true},
 {k:'co2',n:'CO₂',u:'ppm',f:v=>Math.round(v),p:v=>Math.max(0,Math.min((v-400)/1600,1)),w:v=>v>1200},
 {k:'tvoc',n:'TVOC',u:'ppb',f:v=>Math.round(v),p:v=>Math.min(v/1000,1),w:v=>v>500}];

/* ---------- 顶栏设备状态 ---------- */
async function refreshStatus(){
 try{
  const d=await(await fetch('/api/status')).json();
  el('stDot').className='dot '+(d.connected?'on':(d.mode==='AP'?'ap':'off'));
  q('stSub',(d.host||'')+' · '+(d.ip||'')+(d.mode==='STA'?' · '+d.rssi+'dBm':''));
  q('stUp',d.uptime);
 }catch(e){
  el('stDot').className='dot off';q('stSub','设备无响应');
 }
}

/* ---------- 传感大卡(构建一次,此后按 id 更新) ---------- */
function buildSensorGrid(){
 el('sGrid').innerHTML=METRICS.map(m=>
  '<div class="big"><div class="k">'+m.n+'</div><div class="v" id="v_'+m.k+'">--</div>'+
  '<div class="bar"><i id="b_'+m.k+'"></i></div><canvas id="sp_'+m.k+'" width="280" height="56"></canvas></div>').join('');
 hist={};METRICS.forEach(m=>hist[m.k]=[]);
}
function fanTxt(f){return f===1?'正转':(f===2?'反转':'停')}
function ghTxt(s){return s===0?'关棚':(s===180?'开棚':s+'°')}

/* ---------- 从机总览 ---------- */
function metricHTML(d,m){
 if(m.soil){
  if(d.soil===null)return '<div class="mi"><div class="k">土壤</div><div class="v">--</div><div class="bar"><i style="width:0"></i></div></div>';
  /* 原始值端口(F8266-2 模拟 AO):显原始值 0~1023(低=湿 高=干),进度条按满量程,超干阈值变警示 */
  if(d.soilRaw)return '<div class="mi"><div class="k">土壤</div><div class="v">'+d.soil+'</div><div class="bar"><i class="'+(d.soil>=d.soilDry?'warn':'')+'" style="width:'+Math.min(d.soil*100/1023,100)+'%"></i></div></div>';
  return '<div class="mi"><div class="k">土壤</div><div class="v">'+(d.soil===1?'湿润':'干燥')+'</div><div class="bar"><i class="'+(d.soil===1?'ok':'warn')+'" style="width:'+(d.soil===1?100:20)+'%"></i></div></div>';
 }
 if(d[m.k]===null)return '<div class="mi"><div class="k">'+m.n+'</div><div class="v">--</div><div class="bar"><i style="width:0"></i></div></div>';
 return '<div class="mi"><div class="k">'+m.n+'</div><div class="v">'+m.f(d[m.k])+'<small>'+m.u+'</small></div><div class="bar"><i class="'+(m.w(d[m.k])?'warn':'')+'" style="width:'+(m.p(d[m.k])*100)+'%"></i></div></div>';
}
function updateOverview(){
 el('ovSub').textContent='在线 '+PD.filter(d=>d.online).length+' / '+PD.length;
 el('ovGrid').innerHTML=PD.map((d,i)=>
  '<div class="scard'+(d.online?'':' off')+'" onclick="selectSlave('+i+',true)">'+
   '<div class="schead"><span class="dot '+(d.online?'on':'')+'"></span><b>'+(d.slave||('端口'+d.port))+'</b><span class="port">:'+d.port+'</span>'+
   '<span class="stchip '+(d.online?'on':'')+'">'+(d.online?'在线':(d.clients>0?'未上线':'离线'))+'</span></div>'+
   '<div class="mgrid">'+(d.hasData?METRICS.map(m=>metricHTML(d,m)).join(''):'<span class="mut">等待传感数据…</span>')+'</div>'+
   '<div class="achips">'+
    '<span class="achip'+(d.pump===1?' on':'')+'">泵 '+(d.pump===1?'开':'关')+'</span>'+
    '<span class="achip'+(d.fan>0?' act':'')+'">扇 '+fanTxt(d.fan)+'</span>'+
    '<span class="achip'+(d.servo===180?' act':'')+'">棚 '+ghTxt(d.servo)+'</span></div>'+
   '<div class="afoot">收 '+d.rx+' / 发 '+d.tx+'</div>'+
  '</div>').join('');
}

/* ---------- 从机详情 ---------- */
function buildTabs(){
 el('tabs').innerHTML=PD.map((d,i)=>
  '<button class="tab'+(i===sel?' on':'')+'" onclick="selectSlave('+i+')"><span class="dot '+(d.online?'on':'')+'"></span>'+(d.slave||('端口'+d.port))+'</button>').join('');
}
function selectSlave(i,scroll){
 sel=i;
 METRICS.forEach(m=>hist[m.k]=[]);   // 清空趋势采样,重新累积
 if(PD){buildTabs();updateDetail();refreshAuto();}
 if(scroll)el('detailCard').scrollIntoView({behavior:'smooth',block:'start'});
}
function updateDetail(){
 const d=PD&&PD[sel];if(!d)return;
 q('dName',(d.slave||('端口'+d.port))+' :'+d.port);
 el('actPort').textContent='下发端口 '+d.port;
 q('dState',d.hasData?(d.online?'从机在线':'从机已离线'):'等待数据…');
 METRICS.forEach(m=>{
  if(m.soil){
   if(d.soil===null){q('v_soil','--');el('b_soil').style.width='0'}
   else if(d.soilRaw){q('v_soil',String(d.soil));const b=el('b_soil');b.style.width=Math.min(d.soil*100/1023,100)+'%';b.className=d.soil>=d.soilDry?'warn':''}
   else{q('v_soil',d.soil===1?'湿润':'干燥');const b=el('b_soil');b.style.width=d.soil===1?'100%':'20%';b.className=d.soil===1?'ok':'warn'}
  }else{
   const v=d[m.k];
   el('v_'+m.k).innerHTML=v===null?'--':m.f(v)+'<small>'+m.u+'</small>';
   const b=el('b_'+m.k);
   b.style.width=v===null?'0':(m.p(v)*100)+'%';
   b.className=v!==null&&m.w(v)?'warn':'';
  }
 });
 const sp=el('st_pump');sp.textContent=d.pump===1?'开':'关';sp.className='astat'+(d.pump===1?' on':'');
 const sf=el('st_fan');sf.textContent=fanTxt(d.fan);sf.className='astat'+(d.fan>0?' act':'');
 const ss=el('st_servo');ss.textContent=ghTxt(d.servo);ss.className='astat'+(d.servo===180?' act':'');
 // 舵机滑条与回报角度同步(拖动中不强制覆盖)
 if(document.activeElement!==el('servo')){el('servoVal').textContent=d.servo;el('servo').value=d.servo;}
}
/* 近 60 个采样点的迷你趋势线(canvas) */
function pushSpark(){
 const d=PD&&PD[sel];if(!d||!d.hasData)return;
 METRICS.forEach(m=>{
  const v=m.soil?d.soil:d[m.k];
  hist[m.k].push(v===null?null:v);
  if(hist[m.k].length>60)hist[m.k].shift();
  drawSpark(m.k);
 });
}
function drawSpark(k){
 const cv=el('sp_'+k);if(!cv)return;
 const ctx=cv.getContext('2d'),W=cv.width,H=cv.height;
 ctx.clearRect(0,0,W,H);
 const a=hist[k].filter(v=>v!==null);
 if(a.length<2)return;
 let mn=Math.min(...a),mx=Math.max(...a);
 if(mx-mn<1e-6){mn-=.5;mx+=.5}
 const pad=4;let started=false;
 ctx.beginPath();
 hist[k].forEach((v,i)=>{
  if(v===null){started=false;return}
  const x=pad+(W-2*pad)*i/Math.max(hist[k].length-1,1);
  const y=H-pad-(H-2*pad)*(v-mn)/(mx-mn);
  if(started)ctx.lineTo(x,y);else{ctx.moveTo(x,y);started=true}
 });
 ctx.strokeStyle=getComputedStyle(document.documentElement).getPropertyValue('--acc')||'#2f7df6';
 ctx.lineWidth=2;ctx.lineJoin='round';ctx.stroke();
}

/* ---------- 执行器控制(下发到当前选中从机端口) ---------- */
function selPort(){return PD?PD[sel].port:8000}
async function act(cmd){
 if(!PD)return;
 try{
  const body=new URLSearchParams({port:String(selPort()),text:cmd});
  const r=await(await fetch('/api/send',{method:'POST',body:body})).json();
  if(!r.ok)alert('控制失败：'+(r.error||'从机离线'));
 }catch(e){alert('控制失败：网络错误')}
}

/* ---------- 自动控制(每从机独立配置) ---------- */
async function refreshAuto(){
 try{
  const d=await(await fetch('/api/auto?port='+selPort())).json();
  if(!autoInited[d.port]){   // 每端口仅首次填充输入框,避免冲掉正在编辑的阈值
   el('ac_t').value=d.tHigh;el('ac_co2').value=d.co2High;el('ac_h').value=d.hHigh;
   el('ac_sd').value=d.soilDry;el('ac_sw').value=d.soilWet;
   autoInited[d.port]=true;
  }
  /* 土壤双阈值仅对原始值端口生效,两态口置灰禁用 */
  const raw=PD&&PD[sel]&&PD[sel].soilRaw;
  el('ac_sd').disabled=el('ac_sw').disabled=!raw;
  const b=el('autoBtn');
  b.textContent=d.enabled?'自动':'手动';
  b.className='btn '+(d.enabled?'green':'gray');
  el('rules').innerHTML=d.rules.map(r=>
   '<div class="rule'+(r.active?' on':'')+'"><span class="rdot"></span>'+r.name+(r.active?' 已触发':' 未触发')+'</div>').join('');
 }catch(e){}
}
async function saveAuto(){
 const body=new URLSearchParams({port:String(selPort()),enabled:String(el('autoBtn').textContent==='自动'),tHigh:el('ac_t').value,co2High:el('ac_co2').value,hHigh:el('ac_h').value,soilDry:el('ac_sd').value,soilWet:el('ac_sw').value});
 try{
  const r=await(await fetch('/api/auto',{method:'POST',body:body})).json();
  if(r.ok){
   const sb=el('saveBtn');sb.textContent='已保存 ✓';sb.disabled=true;
   setTimeout(()=>{sb.textContent='保存阈值';sb.disabled=false},1500);
   refreshAuto();
  }else alert('保存失败：'+(r.error||''));
 }catch(e){alert('保存失败：网络错误')}
}
async function toggleAuto(){
 const b=el('autoBtn');
 const nowAuto=b.textContent==='自动';
 b.textContent=nowAuto?'手动':'自动';
 b.className='btn '+(nowAuto?'gray':'green');
 await saveAuto();
}

/* ---------- 通信日志二级页 ---------- */
function buildLogCards(){
 el('ports').innerHTML=PD.map((d,i)=>
  '<div class="pcard"><div class="phead"><span class="dot" id="pd'+i+'"></span><b>端口 '+d.port+'</b><span class="pinfo" id="pn'+i+'">无从机</span></div>'+
  '<pre class="plog" id="pl'+i+'"></pre>'+
  '<div class="psend"><input id="pi'+i+'" placeholder="输入要发送的内容"><button class="btn" id="pb'+i+'">发送</button></div>'+
  '<div class="pstat" id="ps'+i+'">收 0 / 发 0</div></div>').join('');
 PD.forEach((d,i)=>{
  el('pb'+i).onclick=()=>sendMsg(i);
  el('pi'+i).onkeydown=e=>{if(e.key==='Enter')sendMsg(i)};
 });
}
function updateLogCards(){
 PD.forEach((d,i)=>{
  el('pd'+i).className='dot '+(d.online?'on':'');
  const info=el('pn'+i);
  if(d.online){info.className='pinfo on';info.textContent=d.slave+' 已上线'}
  else if(d.clients>0){info.className='pinfo';info.textContent='已连接·未上线'}
  else{info.className='pinfo';info.textContent='无从机'}
  const log=el('pl'+i);
  log.textContent=d.log.join('\n')||'等待从机连接…';
  log.scrollTop=log.scrollHeight;
  q('ps'+i,'收 '+d.rx+' / 发 '+d.tx+(d.clients>0?' · 连接 '+d.clients:''));
 });
}
async function sendMsg(i){
 const inp=el('pi'+i),text=inp.value.trim();
 if(!text||!PD)return;
 try{
  const body=new URLSearchParams({port:String(PD[i].port),text:text});
  const r=await(await fetch('/api/send',{method:'POST',body:body})).json();
  if(r.ok){inp.value='';refreshPorts()}
  else alert('发送失败：'+(r.error||'未知错误'));
 }catch(e){alert('发送失败：网络错误')}
}

/* ---------- 轮询 ---------- */
async function refreshPorts(){
 try{
  const a=await(await fetch('/api/ports')).json();
  const first=!PD;
  PD=a;
  if(first){buildTabs();buildLogCards()}
  buildTabs();
  updateOverview();updateDetail();pushSpark();updateLogCards();
 }catch(e){}
}
buildSensorGrid();
refreshStatus();setInterval(refreshStatus,3000);
refreshPorts();setInterval(refreshPorts,1500);
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
    j += ",\"soilRaw\":" + String(PORT_SOIL_RAW[i] ? "true" : "false");   // true=原始值口(数值 0~1023)
    j += ",\"soilDry\":" + String(auto_ctrl::getConfig(i).soilDry, 0);    // 原始值口干阈值(网页警示色用)
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

// /api/auto?port=800N GET:返回指定端口自动控制配置 + 各规则触发状态(缺省 port=8000)
static long autoIdxFromArg() {
  long port = server.hasArg("port") ? server.arg("port").toInt() : (long)PORT_BASE;
  if (port < PORT_BASE || port >= PORT_BASE + PORT_COUNT) return -1;
  return port - PORT_BASE;
}

static void handleGetAuto() {
  long idx = autoIdxFromArg();
  if (idx < 0) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"端口不在监听范围内\"}");
    return;
  }
  auto_ctrl::Config c = auto_ctrl::getConfig(idx);
  String j;
  j.reserve(256);
  j += "{\"port\":" + String(PORT_BASE + idx);
  j += ",\"enabled\":" + String(c.enabled ? "true" : "false");
  j += ",\"tHigh\":" + String(c.tHigh);
  j += ",\"co2High\":" + String(c.co2High);
  j += ",\"hHigh\":" + String(c.hHigh);
  j += ",\"soilWet\":" + String(c.soilWet);
  j += ",\"soilDry\":" + String(c.soilDry);
  j += ",\"fanDir\":" + String(auto_ctrl::fanDir(idx));   // 0=停 1=正转 2=反转
  j += ",\"pumpActive\":" + String(auto_ctrl::pumpActive(idx) ? "true" : "false");
  j += ",\"servoActive\":" + String(auto_ctrl::servoActive(idx) ? "true" : "false");
  j += ",\"rules\":[";
  for (uint8_t i = 0; i < auto_ctrl::ruleCount(); i++) {
    if (i) j += ",";
    auto_ctrl::RuleState r = auto_ctrl::ruleState(idx, i);
    j += "{\"name\":\"" + jsonEsc(r.name) + "\"";
    j += ",\"active\":" + String(r.active ? "true" : "false") + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

// /api/auto POST:保存指定端口配置(enabled/tHigh/co2High/hHigh + port)并写 NVS
static void handleSetAuto() {
  long idx = autoIdxFromArg();
  if (idx < 0) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"端口不在监听范围内\"}");
    return;
  }
  if (!server.hasArg("tHigh") || !server.hasArg("co2High") || !server.hasArg("hHigh") ||
      !server.hasArg("soilWet") || !server.hasArg("soilDry")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"缺少阈值参数\"}");
    return;
  }
  auto_ctrl::Config c;
  c.enabled = server.arg("enabled") == "true";
  c.tHigh   = server.arg("tHigh").toFloat();
  c.co2High = server.arg("co2High").toFloat();
  c.hHigh   = server.arg("hHigh").toFloat();
  c.soilWet = server.arg("soilWet").toFloat();
  c.soilDry = server.arg("soilDry").toFloat();
  if (c.soilDry <= c.soilWet) {   // 干阈值须大于湿阈值,否则滞回带为负规则锁死
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"土壤干阈值须大于湿阈值\"}");
    return;
  }
  auto_ctrl::setConfig(idx, c);
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
