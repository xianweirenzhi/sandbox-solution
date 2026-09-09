<script>
/* ============ 模拟层(仅本文件有):按 esp32-2 固件 v2 逻辑仿真 6 从机全部接入 ============
   页面 HTML/CSS/JS 与固件内嵌网页逐字节一致,此处只替换 fetch 网络层:
   - /api/status /api/ports /api/send 与 v1 相同
   - /api/auto 带端口参数,每端口独立配置与规则状态(auto_ctrl 六实例)
   场景:端口0 温度绕 30℃ 摆动(高温通风周期触发)/ 端口1 土壤持续干燥(自动浇水)/
        端口4 CO₂ 持续偏高(高CO₂通风常触发)/ 其余平稳。 */
(function(){
'use strict';

var BOOT_OFFSET_S = 5*3600 + 23*60 + 41;
var T0 = Date.now();
function upSec(){ return Math.floor((Date.now()-T0)/1000) + BOOT_OFFSET_S; }
function p2(n){ return (n<10?'0':'')+n; }
function fmtTs(s){ return p2(Math.floor(s/3600))+':'+p2(Math.floor(s%3600/60))+':'+p2(s%60); }
function uptimeStr(){ var s=upSec(); return Math.floor(s/86400)+'d '+p2(Math.floor(s%86400/3600))+':'+p2(Math.floor(s%3600/60))+':'+p2(s%60); }

/* ---------- 端口状态 ---------- */
var PORT_MAX = 8;
function logAt(p, sec, text){ p.log.push(fmtTs(sec)+' '+text); if(p.log.length>PORT_MAX) p.log.shift(); }
function logNow(p, text){ logAt(p, upSec(), text); }

function mkPort(i, name, ip, connAt, onAt, dataAt, init){
  var p = {port:8000+i, clients:1, online:true, slave:name, rx:0, tx:0,
           hasData:true, t:0, h:0, lux:0, soil:1, co2:0, tvoc:0,
           pump:0, fan:0, servo:0, log:[]};
  for(var k in init) p[k]=init[k];
  p._base = {t:init.t, h:init.h, lux:init.lux, co2:init.co2, tvoc:init.tvoc};
  p.rx = 1 + Math.max(0, Math.floor((upSec()-dataAt)/2));
  logAt(p, connAt, '● 从机接入 '+ip);
  logAt(p, onAt,   '★ '+name+' 已上线');
  logAt(p, onAt,   '← @'+name+' online/');
  logAt(p, dataAt, '⚙ 开始接收传感数据');
  return p;
}

var ports = [
  mkPort(0,'F8266-1','192.168.1.121', 5,  7,  9,  {t:32.1, h:57.8, lux:328,  soil:1, co2:612,  tvoc:43, fan:1, tx:159}),
  mkPort(1,'F8266-2','192.168.1.122', 8,  10, 12, {t:23.7, h:66.2, lux:8460, soil:0, co2:588,  tvoc:37, pump:1, tx:1}),
  mkPort(2,'F8266-3','192.168.1.123', 11, 13, 15, {t:27.9, h:61.5, lux:1240, soil:1, co2:655,  tvoc:52}),
  mkPort(3,'F8266-4','192.168.1.124', 14, 16, 18, {t:25.2, h:54.0, lux:305,  soil:1, co2:545,  tvoc:28}),
  mkPort(4,'F8266-5','192.168.1.125', 17, 19, 21, {t:28.6, h:63.8, lux:2100, soil:1, co2:1352, tvoc:61, fan:1, tx:1}),
  mkPort(5,'F8266-6','192.168.1.126', 20, 22, 24, {t:24.8, h:58.9, lux:530,  soil:1, co2:590,  tvoc:33})
];

/* 端口 0 历史:整夜高温通风反复触发,环形日志只剩近期 FAN FWD/STOP 补发记录 */
(function(){
  var marks = [18513,18633,18753,18873,18993,19113,19233,19353];
  ports[0].log = [];
  for(var k=0;k<marks.length;k++) logAt(ports[0], marks[k], '→ @'+(k%2 ? 'FAN FWD' : 'FAN STOP')+'/');
})();
/* 端口 1/4 历史:自动浇水/CO₂ 通风曾各补发一条 */
logAt(ports[1], 19000, '→ @PUMP ON/');
logAt(ports[4], 19000, '→ @FAN FWD/');

/* ---------- 从机执行命令 ---------- */
function applyCmd(p, text){
  if(text==='PUMP ON') p.pump=1;
  else if(text==='PUMP OFF') p.pump=0;
  else if(text==='FAN FWD') p.fan=1;
  else if(text==='FAN REV') p.fan=2;
  else if(text==='FAN STOP') p.fan=0;
  else if(/^SERVO \d+$/.test(text)) p.servo = Math.max(0, Math.min(180, parseInt(text.slice(6),10)));
}

/* ---------- /api/send ---------- */
function mockSend(opts){
  var b = new URLSearchParams((opts&&opts.body)||'');
  var port = parseInt(b.get('port'),10), text = b.get('text');
  if(!b.has('port') || !b.has('text') || !text) return {ok:false, error:'缺少 port/text 参数'};
  if(isNaN(port) || port<8000 || port>=8006) return {ok:false, error:'端口不在监听范围内'};
  var p = ports[port-8000];
  if(p.clients===0) return {ok:false, error:'该端口无已连接从机'};
  applyCmd(p, text);
  p.tx++;
  logNow(p, '→ @'+text+'/');
  return {ok:true, error:''};
}

/* ---------- auto_ctrl 六端口独立实例复刻 ---------- */
var AC_CONFIRM_N = 3, AC_OVER_FACTOR = 1.2;
function mkAc(){ return {enabled:true, tHigh:30, co2High:1200, hHigh:70, fanDir:0, pumpActive:false,
  rules:[{name:'高温通风',hi:0,lo:0,active:false},{name:'高CO₂通风',hi:0,lo:0,active:false},
         {name:'高湿排湿',hi:0,lo:0,active:false},{name:'土壤浇水',hi:0,lo:0,active:false}],
  ghHi:0, ghLo:0, ghActive:false}; }
var acs = [mkAc(),mkAc(),mkAc(),mkAc(),mkAc(),mkAc()];
acs[0].rules[0].active = true; acs[0].rules[0].hi = 9; acs[0].fanDir = 1;  // 与端口0历史一致
acs[1].rules[3].active = true; acs[1].rules[3].hi = 9; acs[1].pumpActive = true; // 干燥已浇水
acs[4].rules[1].active = true; acs[4].rules[1].hi = 9; acs[4].fanDir = 1;  // CO₂ 通风中

function sendCmd(idx, txt){
  var p = ports[idx];
  applyCmd(p, txt); p.tx++; logNow(p, '→ @'+txt+'/');
}
function upd(r, over, safe){
  if(over){ r.hi++; r.lo=0; } else if(safe){ r.lo++; r.hi=0; } else return;
  if(!r.active && r.hi>=AC_CONFIRM_N) r.active=true;
  else if(r.active && r.lo>=AC_CONFIRM_N) r.active=false;
}
function updGh(ac, over, safe){   // 大棚独立防抖(else return 只跳出本函数,不阻断后续校正)
  if(over){ ac.ghHi++; ac.ghLo=0; } else if(safe){ ac.ghLo++; ac.ghHi=0; } else return;
  if(!ac.ghActive && ac.ghHi>=AC_CONFIRM_N) ac.ghActive=true;
  else if(ac.ghActive && ac.ghLo>=AC_CONFIRM_N) ac.ghActive=false;
}
function acTick(){
  for(var idx=0; idx<ports.length; idx++){
    var ac = acs[idx], p = ports[idx];
    if(!ac.enabled) continue;
    if(!p.hasData) continue;
    upd(ac.rules[0], p.t>=ac.tHigh,     p.t<=ac.tHigh-2);
    upd(ac.rules[1], p.co2>=ac.co2High, p.co2<=ac.co2High-200);
    upd(ac.rules[2], p.h>=ac.hHigh,     p.h<=ac.hHigh-5);
    upd(ac.rules[3], p.soil===0,        p.soil===1);
    updGh(ac,
      (p.t>=ac.tHigh && p.t>=ac.tHigh*AC_OVER_FACTOR) ||
      (p.co2>=ac.co2High && p.co2>=ac.co2High*AC_OVER_FACTOR) ||
      (p.h>=ac.hHigh && p.h>=ac.hHigh*AC_OVER_FACTOR),
      p.t<=ac.tHigh && p.co2<=ac.co2High && p.h<=ac.hHigh);
    var heat = ac.rules[0].active || ac.rules[1].active, wet = ac.rules[2].active;
    ac.pumpActive = ac.rules[3].active;
    ac.fanDir = heat ? 1 : (wet ? 2 : 0);
    if(p.online && p.hasData){
      if(p.fan!==ac.fanDir) sendCmd(idx, ac.fanDir===1?'FAN FWD':(ac.fanDir===2?'FAN REV':'FAN STOP'));
      var wp = ac.pumpActive?1:0;
      if(p.pump!==wp) sendCmd(idx, wp?'PUMP ON':'PUMP OFF');
      var ws = ac.ghActive?180:0;
      if(p.servo!==ws) sendCmd(idx, ws?'SERVO 180':'SERVO 0');
    }
  }
}

/* ---------- 传感数据慢漂移 ---------- */
function r1(x){ return Math.round(x*10)/10; }
function walk(){
  var s = upSec();
  ports.forEach(function(p,i){
    p.rx++;
    if(i===0){
      p.t    = r1(30 + 2.2*Math.sin(2*Math.PI*(s-19353)/240) + (Math.random()-.5)*.2);
      p.h    = r1(57.8 + 2.5*Math.sin(s/97) + (Math.random()-.5)*.6);
      p.lux  = Math.max(0, Math.round(328 + 40*Math.sin(s/61) + (Math.random()-.5)*30));
      p.co2  = Math.max(400, Math.round(612 + 60*Math.sin(s/83) + (Math.random()-.5)*40));
      p.tvoc = Math.max(0,  Math.round(43 + 8*Math.sin(s/71) + (Math.random()-.5)*8));
    }else{
      var B = p._base;
      p.t    = r1(p.t + (B.t-p.t)*.05 + (Math.random()-.5)*.3);
      p.h    = r1(p.h + (B.h-p.h)*.05 + (Math.random()-.5)*.6);
      p.lux  = Math.max(0, Math.round(p.lux + (B.lux-p.lux)*.05 + (Math.random()-.5)*60));
      p.co2  = Math.max(400, Math.round(p.co2 + (B.co2-p.co2)*.05 + (Math.random()-.5)*(i===4?20:30)));
      p.tvoc = Math.max(0,  Math.round(p.tvoc + (B.tvoc-p.tvoc)*.05 + (Math.random()-.5)*6));
    }
  });
  acTick();
}
setInterval(walk, 2000);

/* ---------- /api/status ---------- */
function mockStatus(){
  return {mode:'STA', connected:true, ssid:'ESP-TEST', ip:'192.168.1.109', gw:'192.168.1.1',
          mask:'255.255.255.0', mac:'F4:12:FA:6C:5E:12',
          rssi:Math.round(-55 + 2*Math.sin(upSec()/47) + (Math.random()-.5)), ch:6,
          host:'esp32s3-2', uptime:uptimeStr(),
          heapFree:221, heapSize:320, psramFree:8101, psramSize:8192,
          flashMB:16, flashMHz:80, chip:'ESP32-S3 rev3 x2 核', cpuMHz:240,
          tempC:'45.8', sdk:'v4.4.7-1-gbd42a0a'};
}

/* ---------- /api/ports ---------- */
function snap(p){
  return {port:p.port, clients:p.clients, online:p.online, slave:p.slave, rx:p.rx, tx:p.tx,
          hasData:p.hasData, t:p.t, h:p.h, lux:p.lux, soil:p.soil, co2:p.co2, tvoc:p.tvoc,
          pump:p.pump, fan:p.fan, servo:p.servo, log:p.log.slice(0, PORT_MAX)};
}

/* ---------- /api/auto 六端口独立 ---------- */
function portFromQ(qs){ var v=(qs||'').match(/port=(\d+)/); return v?parseInt(v[1],10):8000; }
function mockAuto(port){
  if(isNaN(port) || port<8000 || port>=8006) return {ok:false, error:'端口不在监听范围内'};
  var ac = acs[port-8000];
  return {port:port, enabled:ac.enabled, tHigh:ac.tHigh, co2High:ac.co2High, hHigh:ac.hHigh,
          fanDir:ac.fanDir, pumpActive:ac.pumpActive, servoActive:ac.ghActive,
          rules:ac.rules.map(function(r){ return {name:r.name, active:r.active}; })};
}
function mockSetAuto(opts){
  var b = new URLSearchParams((opts&&opts.body)||'');
  var port = parseInt(b.get('port'),10);
  if(isNaN(port) || port<8000 || port>=8006) return {ok:false, error:'端口不在监听范围内'};
  if(!b.has('tHigh') || !b.has('co2High') || !b.has('hHigh')) return {ok:false, error:'缺少阈值参数'};
  var ac = acs[port-8000];
  ac.enabled  = b.get('enabled')==='true';
  ac.tHigh    = parseFloat(b.get('tHigh'))   || 0;
  ac.co2High  = parseFloat(b.get('co2High')) || 0;
  ac.hHigh    = parseFloat(b.get('hHigh'))   || 0;
  return {ok:true};
}

/* ---------- fetch 拦截 ---------- */
function _r(o){ return Promise.resolve({ok:true, json:function(){ return Promise.resolve(o); }}); }
window.fetch = function(u, opts){
  u = String(u);
  if(u.indexOf('/api/status')===0) return _r(mockStatus());
  if(u.indexOf('/api/ports')===0)  return _r(ports.map(snap));
  if(u.indexOf('/api/send')===0)   return _r(mockSend(opts));
  if(u.indexOf('/api/auto')===0){
    if(opts && opts.method==='POST') return _r(mockSetAuto(opts));
    return _r(mockAuto(portFromQ(u.split('?')[1])));
  }
  return _r({});
};

/* ---------- 角标提示(非固件内容) ---------- */
var badge = document.createElement('div');
badge.textContent = '⚙ 模拟环境 · 6 从机在线';
badge.style.cssText = 'position:fixed;right:10px;bottom:10px;z-index:99;background:rgba(52,73,94,.85);'+
                      'color:#fff;font:12px/1 "Microsoft YaHei",sans-serif;padding:6px 10px;'+
                      'border-radius:12px;opacity:.85;pointer-events:none';
document.body.appendChild(badge);
})();
</script>
