(function(root){
'use strict';
// These samples describe UI states, not vehicle-specific fault detection.
const alert=(field,reason,severity='warning')=>({field,reason,severity});
const scenario=(id,label,values={},alerts=[])=>({id,label,values,alerts});
const pressure=scenario('pressure-high','压力超限',{boost:2.60},[alert('boost','BOOST HIGH')]);
const oilHot=scenario('oil-hot','油温过高',{oiltemp:130},[alert('oiltemp','OIL TEMP HIGH')]);
const oilLow=scenario('oil-low','油压过低',{oilpress:0.5},[alert('oilpress','OIL PRESS LOW','critical')]);
const waterHot=scenario('coolant-hot','水温过高',{coolant:112},[alert('coolant','COOLANT HIGH')]);
const batteryLow=scenario('voltage-low','电压过低',{battery:10.8},[alert('battery','VOLTAGE LOW')]);
const batteryHigh=scenario('voltage-high','电压过高',{battery:16.4},[alert('battery','VOLTAGE HIGH')]);
const normal=scenario('normal','正常');
const pages={
 gear:[normal,scenario('shift','换挡提示',{rpm:7200},[alert('rpm','SHIFT / 7,200','advisory')]),scenario('rpm-high','转速过高',{rpm:7900},[alert('rpm','RPM LIMIT','critical')]),oilHot,oilLow],
 boost:[normal,pressure],analog:[normal,pressure],coolant:[normal,waterHot],oiltemp:[normal,oilHot],oilpress:[normal,oilLow],
 triple:[normal,pressure,oilHot,oilLow,scenario('multiple','多项异常',{boost:2.60,oiltemp:130,oilpress:0.5},[alert('oilpress','OIL PRESS LOW','critical'),alert('oiltemp','OIL TEMP HIGH'),alert('boost','BOOST HIGH')])],
 multi:[normal,oilLow,oilHot,waterHot,batteryLow,batteryHigh,pressure,scenario('multiple','多项异常',{oilpress:0.5,oiltemp:130,battery:10.8},[alert('oilpress','OIL PRESS LOW','critical'),alert('oiltemp','OIL TEMP HIGH'),alert('battery','VOLTAGE LOW')])],
 tyres:[normal,scenario('tyre-low','胎压过低'),scenario('tyre-high','胎压过高'),scenario('tyre-hot','胎温过高'),scenario('multiple','多轮异常')],
 speed:[normal,scenario('speed-limit','超速提醒（启用）')],
 gforce:[normal,scenario('g-limit','G 值提醒（启用）')],
 heart:[normal,scenario('heart-high','心率偏高提醒'),scenario('heart-low','心率偏低提醒'),scenario('no-data','心率无数据')],
 music:[normal,scenario('media-unavailable','媒体不可用')],
 display:[normal],
 bluetooth:[normal,scenario('connection-lost','连接丢失'),scenario('no-device','未配对设备')],
 pair:[normal,scenario('pair-failed','配对失败'),scenario('pair-expired','配对码过期')]
};
const limits={speed:110,gforce:1.0,heartLow:50,heartHigh:150};
const baseline={gear:4,rpm:4800,boost:0.68,coolant:92,oiltemp:104,oilpress:3.8,battery:14.2,speed:105,lat:0.52,lon:0.18,heart:105,low:60,peak:165,average:105};
const wheels=['FL','FR','RL','RR'];
function selected(id,s){
 const entries=pages[id]||[normal];
 const requested=s.scenario??s.scenarios?.[id]??(s.alarm?entries[1]?.id:'normal');
 return entries.find(entry=>entry.id===requested)||normal;
}
function resolve(id,s){
 const entry=selected(id,s),limit={...limits,...s.limits};
 const values={...baseline,...entry.values},alerts=[...entry.alerts];
 const tyres={FL:{pressure:2.2,temp:35},FR:{pressure:2.2,temp:35},RL:{pressure:2.1,temp:37},RR:{pressure:2.1,temp:37}};
 const wheel=wheels.includes(s.tyreWheel)?s.tyreWheel:'RR';
 if(id==='tyres'){
  const add=(at,kind)=>{
   if(kind==='low')tyres[at].pressure=0.5;
   if(kind==='high')tyres[at].pressure=3.4;
   if(kind==='hot')tyres[at].temp=85;
   alerts.push(alert(at+'.'+(kind==='hot'?'temp':'pressure'),at+' / '+(kind==='hot'?'TEMP HIGH':kind==='low'?'PRESS LOW':'PRESS HIGH'),kind==='low'?'critical':'warning'));
  };
  if(entry.id==='tyre-low')add(wheel,'low');
  if(entry.id==='tyre-high')add(wheel,'high');
  if(entry.id==='tyre-hot')add(wheel,'hot');
  if(entry.id==='multiple'){add('FL','low');add('RR','high');add('RL','hot');}
 }
 if(id==='speed'){
  values.speed=Number.isFinite(s.previewSpeed)?Math.min(root.NC_FOUNDATIONS.road.maximumPreviewSpeed,Math.max(0,s.previewSpeed)):entry.id==='speed-limit'?limit.speed+15:baseline.speed;
  if(entry.id==='speed-limit'&&values.speed>limit.speed)alerts.push(alert('speed','SPEED LIMIT','advisory'));
 }
 if(id==='gforce'&&entry.id==='g-limit'){
  values.lat=Math.min(1.9,limit.gforce+0.2);values.lon=0.18;alerts.push(alert('gforce','G LIMIT','advisory'));
 }
 if(id==='heart'&&entry.id==='heart-high'){
  values.heart=limit.heartHigh+15;values.peak=Math.max(values.peak,values.heart);values.average=153;
  alerts.push(alert('heart','ABOVE SET RANGE','advisory'));
 }
 if(id==='heart'&&entry.id==='heart-low'){
  values.heart=Math.max(20,limit.heartLow-10);values.low=values.heart;values.average=46;
  alerts.push(alert('heart','BELOW SET RANGE','advisory'));
 }
 const vehicle=!['heart','music','display','bluetooth','pair'].includes(id);
 const connected=vehicle?s.signal!==false:id==='heart'?s.heartSignal!==false&&entry.id!=='no-data':true;
 const rank={critical:0,warning:1,advisory:2};
 const active=connected?alerts.sort((a,b)=>rank[a.severity]-rank[b.severity]):[];
 return {id,scenario:entry.id,connected,vehicle,values,tyres,limits:limit,alerts:active,
  mediaAvailable:id!=='music'||entry.id!=='media-unavailable',
  connectionLost:id==='bluetooth'&&entry.id==='connection-lost',
  paired:s.paired!==false&&entry.id!=='no-device',
  pairingFailed:id==='pair'&&['pair-failed','pair-expired'].includes(entry.id)};
}
root.NC_SCENARIOS={pages,limits,wheels,selected,resolve};
})(typeof window!=='undefined'?window:globalThis);
