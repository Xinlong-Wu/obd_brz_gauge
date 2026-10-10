(function(root){
'use strict';
const F=root.NC_FOUNDATIONS,C=F.colors;
const PAGES=[
{id:'gear',group:'车辆仪表',title:'挡位 / 转速',english:'GEAR & RPM',desc:'大挡位与分段转速弧；温度和油压位于下方。'},
{id:'boost',group:'车辆仪表',title:'增压 / 真空',english:'MANIFOLD',desc:'270°压力弧、细指针与偏下方读数，替代斜坡面积图。'},
{id:'coolant',group:'车辆仪表',title:'冷却液温度',english:'COOLANT',desc:'纵向温度柱、数字和状态文字；高温范围用红色刻度表示，柱体只显示当前温度。'},
{id:'oiltemp',group:'车辆仪表',title:'发动机油温',english:'OIL TEMPERATURE',desc:'温度弧与主读数居中对齐，保留暖机与报警状态。'},
{id:'oilpress',group:'车辆仪表',title:'发动机油压',english:'OIL PRESSURE',desc:'左侧偏心机械表与右侧读数区各自留出空间；压力、单位和状态共用右侧对齐轴，圆弧与数字保持间距。'},
{id:'triple',group:'车辆仪表',title:'三项综合监测',english:'POWERTRAIN',desc:'主增压读数与下方油温、油压双分区。'},
{id:'multi',group:'车辆仪表',title:'多项综合监测',english:'ENGINE VITALS',desc:'中间右侧以 TPS / B.P 标签和大读数组成整体切换区；点击标签、数字或留白均切换指标，琥珀色和下划线标明当前项。'},
{id:'tyres',group:'车辆仪表',title:'四轮胎压 / 胎温',english:'TYRE MONITOR',desc:'原创敞篷跑车俯视线稿，四轮定位；文字与颜色共同提示异常。'},
{id:'speed',group:'动态与健康',title:'车速 / 巡航',english:'SPEED',desc:'路边树木与建筑沿道路透视掠过，车速越高，移动与出现频率越快；停车和断连时停止。车速读数为主，巡航设定为辅。'},
{id:'gforce',group:'动态与健康',title:'G 力 / 当前点',english:'G-FORCE',desc:'同心圆与十字刻度：内圈 1g、外圈 2g，以当前点直接表达方向；提醒触发时保留虚线阈值圈与当前点强调，表盘底部不显示文字。'},
{id:'heart',group:'动态与健康',title:'心率 / 趋势',english:'DRIVER PULSE',desc:'实时心率、低值、峰值、1分钟统计与脉搏波形。'},
{id:'analog',group:'车辆仪表',title:'机械压力表',english:'PRESSURE / ANALOG',desc:'参考 01／02／04 的表盘构图：居中的 270°针式刻度，BOOST 小标识融入表盘内部，下方直接显示压力读数与单位。'},
{id:'music',group:'媒体与系统',title:'音乐 / 歌词',english:'NOW PLAYING',desc:'圆形主播放键配轻量切歌图标，保留唱片、歌词与进度信息。'},
{id:'display',group:'媒体与系统',title:'日间 / 夜间亮度',english:'ILLUMINATION',desc:'圆形加减键与精密模式键，分别调节日间和夜间亮度。'},
{id:'bluetooth',group:'媒体与系统',title:'蓝牙 / 遥控',english:'CONNECTIONS',desc:'拨片式开关配轻量返回与配对操作，保留设备状态和配对管理。'},
{id:'pair',group:'媒体与系统',title:'配对码',english:'PAIR DEVICE',desc:'分组配对码、配对确认和返回导航。'}
];
const defaults={page:'gear',night:false,day:85,dark:35,alarm:false,signal:true,visible:true,keyboard:true,paired:true,playing:true,track:0,progress:69,dialog:false,channel:'TPS',scenarios:{},tyreWheel:'RR',heartSignal:true,pairCode:'1234 5678',previewSpeed:null,limits:{...root.NC_SCENARIOS.limits}};
const songs=[{title:'Night Drive',artist:'ROADSTER RADIO',lyric:'沿着夜色，驶向下一个弯道',previous:'把城市留在身后',next:'让风穿过自由的方向',duration:189},{title:'Open Sky',artist:'ROADSTER RADIO',lyric:'抬头是星光，前方是远方',previous:'驶过漫长的街灯',next:'把时间交给这条路',duration:205}];
let state={...defaults,scenarios:{},limits:{...defaults.limits}};
const {esc,t,r,path,pt,arc,art,metric,header,shell,chip,btn,controlHit,roundButton,toggleControl,readoutControl,icon,mediaButton,trend,readout,statusText,alertInk,alertField,alertMessage,alertRim,needle,liveArc,tachometer,roadMotion,roadProjection,roadScene}=root.NC_COMPONENTS;
const S=root.NC_SCENARIOS;
function render(id,s=state){
const m=S.resolve(id,s),v=m.connected,n=m.values,dash='—';
const find=k=>m.alerts.find(a=>a.field===k),ink=(k,normal='text')=>find(k)?alertInk(find(k)):normal;
const readState=k=>!v?'disconnected':find(k)&&find(k).severity!=='advisory'?'warning':'normal';
function reading(k,x,y,label,value,unit,compact=false){
 const alert=find(k),name=compact&&alert?alert.reason:label;
 return alertField(k,metric(x,y,name,value,unit,compact,ink(k),readState(k)),alert);
}
let a=shell(!v?(id==='heart'?'NO SENSOR':'NO SIGNAL'):'');
switch(id){
case 'gear':{
 a+=tachometer(n.rpm,v,ink('rpm'))+t(360,172,'MX-5 / NC',F.typography.sizes.label,'amber');
 a+=readout({x:200,y:240,label:'GEAR',value:n.gear,unit:'',state:v?'normal':'disconnected'});
 a+=alertField('rpm',t(360,472,v?n.rpm.toLocaleString('en-US')+' RPM':'RPM',F.typography.sizes.support,ink('rpm','muted')),find('rpm'));
 if(v)a+=alertField('oiltemp',t(256,535,'OIL '+n.oiltemp+'°C',F.typography.sizes.support,ink('oiltemp')),find('oiltemp'))+alertField('oilpress',t(466,535,'PRESS '+n.oilpress.toFixed(1)+' bar',F.typography.sizes.support,ink('oilpress')),find('oilpress'));
 else a+=t(360,535,'WAITING FOR VEHICLE',F.typography.sizes.support,'muted');
 if(m.alerts.length)a+=alertMessage(m.alerts,590);break;}
case 'boost':{
 const angle=135+(n.boost+1)/4*270;
 a+=art('boost');
 if(v)a+=liveArc(360,365,274,135,angle,ink('boost','amber'),12)+needle(360,365,205,angle,ink('boost','amber'));
 a+=t(360,178,'MANIFOLD',F.typography.sizes.title,'amber')+t(360,325,'VACUUM / BOOST',F.typography.sizes.small,'muted')+reading('boost',200,375,'BOOST',n.boost.toFixed(2),'bar');
 a+=alertMessage(m.alerts);break;}
case 'coolant':{
 a+=header('COOLANT','ENGINE THERMAL')+art('thermal');
 if(v){const y=498-Math.min(1,Math.max(0,(n.coolant-60)/60))*288;
  a+='<g data-live-indicator="temperature">'+r(154,y,20,498-y,ink('coolant','amber'),10)+path('M137 '+y+'h49',ink('coolant'),3)+'</g>';
 }
 a+=reading('coolant',265,220,'WATER',n.coolant,'°C')+t(420,530,!v?'WAITING':find('coolant')?'TEMP HIGH':'STABLE',F.typography.sizes.label,!v?'muted':find('coolant')?'red':'green')+t(112,208,'120',F.typography.sizes.support,'red')+t(112,480,'60',F.typography.sizes.support,'muted');break;}
case 'oiltemp':{
 a+=art('oil');
 if(v)a+=liveArc(360,360,246,135,135+Math.min(1,Math.max(0,(n.oiltemp-40)/100))*270,ink('oiltemp','amber'));
 a+=t(360,172,'OIL TEMPERATURE',F.typography.sizes.title,'amber')+reading('oiltemp',200,242,'ENGINE OIL',n.oiltemp,'°C')+t(360,540,!v?'WAITING':find('oiltemp')?'OIL TEMP HIGH':'WARM / STABLE',F.typography.sizes.support,!v?'muted':find('oiltemp')?'red':'green');break;}
case 'oilpress':{
 const angle=130+n.oilpress/6*260;
 a+=header('OIL PRESSURE','LUBRICATION')+'<g data-oilpress-dial="eccentric">'+art('press');
 if(v)a+=liveArc(220,355,160,130,angle,ink('oilpress','amber'),8)+needle(220,355,125,angle,ink('oilpress','text'));
 a+='</g><g data-oilpress-readout="numeric">'+alertField('oilpress',readout({x:360,y:240,label:'PRESSURE',value:n.oilpress.toFixed(1),unit:'bar',size:'eccentric',state:readState('oilpress')}),find('oilpress'))+statusText(520,540,!v?'WAITING':find('oilpress')?'LOW PRESSURE':'STABLE',readState('oilpress'))+'</g>';break;}
case 'triple':{
 a+=header('POWERTRAIN','THREE CHANNEL VIEW')+art('triple');
 if(v)a+='<g data-live-indicator="boost-bar">'+r(112,196,496*Math.min(1,(n.boost+1)/4),10,ink('boost','amber'),5)+'</g>';
 a+=alertField('boost',readout({x:200,y:215,label:find('boost')?'BOOST HIGH':'BOOST',value:n.boost.toFixed(2),unit:'bar',state:readState('boost')}),find('boost'));
 a+=reading('oiltemp',130,492,'OIL TEMP',n.oiltemp,'°C',true)+reading('oilpress',370,492,'OIL PRESS',n.oilpress.toFixed(1),'bar',true);
 a+=alertMessage(m.alerts,650);break;}
case 'multi':{
 a+=header('ENGINE VITALS')+path('M108 314H612M360 162V308M108 466H612M360 470V610');
 a+=reading('oilpress',115,166,'OIL PRESS',n.oilpress.toFixed(1),'bar',true)+reading('oiltemp',375,166,'OIL TEMP',n.oiltemp,'°C',true)+reading('battery',115,477,'BATTERY',n.battery.toFixed(1),'V',true)+reading('coolant',375,477,'COOLANT',n.coolant,'°C',true);
 a+=alertField('boost',t(206,336,v?n.boost.toFixed(2):dash,F.typography.sizes.boost,ink('boost','amber'),'middle',F.typography.number)+t(206,427,find('boost')?'BOOST HIGH':'BOOST / bar',F.typography.sizes.small,ink('boost','muted')),find('boost'))+readoutControl(s.channel==='TPS'?'TPS':'B.P',v?(s.channel==='TPS'?'38%':'21%'):dash);
 a+=alertMessage(m.alerts);break;}
case 'tyres':{
 a+=header('TYRE MONITOR','MX-5 / ROADSTER')+art('roadster');
 [[186,252,'FL'],[534,252,'FR'],[186,430,'RL'],[534,430,'RR']].forEach(([x,y,wheel])=>{
  const p=find(wheel+'.pressure'),temp=find(wheel+'.temp'),alert=p||temp,q=m.tyres[wheel];
  a+='<g data-wheel="'+wheel+'">'+t(x,y,wheel,F.typography.sizes.small,alert?alertInk(alert):'muted');
  a+=alertField(wheel+'.pressure',t(x,y+29,v?q.pressure.toFixed(1):dash,F.typography.sizes.tyre,p?alertInk(p):'text','middle',F.typography.number),p);
  a+=alertField(wheel+'.temp',t(x,y+112,'bar / '+(v?q.temp:dash)+'°C',F.typography.sizes.small,temp?alertInk(temp):'muted'),temp);
  if(alert)a+=path('M'+(x-58)+' '+(y+143)+'H'+(x+58),alertInk(alert),3,'data-wheel-alert="'+wheel+'"');
  a+='</g>';
 });
 a+=alertMessage(m.alerts,610);break;}
case 'speed':{
 const alert=find('speed');
 a+=header('SPEED','ROADSTER / TOURING')+roadScene(n.speed,v,s.roadDistance)+path('M158 575L292 380M562 575L428 380','line',3)+path('M360 550V524M360 490V474','amber',5)+reading('speed',200,204,'VELOCITY',n.speed,'km/h');
 a+=chip(224,490,272,!v?'CRUISE UNAVAILABLE':alert?'SPEED LIMIT':n.speed===0?'CRUISE STANDBY':'CRUISE ACTIVE',alert?'amber':'panel')+t(360,551,!v?'SET — km/h':alert?'LIMIT '+m.limits.speed+' km/h':'SET 100 km/h',F.typography.sizes.title,!v||n.speed===0?'muted':alert?'amber':'green');break;}
case 'gforce':{
 const radius=238,lat=n.lat,lon=n.lon,alert=find('gforce');
 a+=header('G-FORCE');
 a+='<circle data-g-meter-ring="2" cx="360" cy="360" r="'+radius+'" fill="none" stroke="'+C.amber+'" stroke-width="3"/><circle data-g-meter-ring="1" cx="360" cy="360" r="'+(radius/2)+'" fill="none" stroke="'+C.line+'" stroke-width="2"/>';
 a+=path('M122 360H598M360 122V598','line',2)+path('M122 360H152M568 360H598M360 122V152M360 568V598','amber',6,'stroke-linecap="round"')+path('M241 360H258M462 360H479M360 241V258M360 462V479','amber',4,'stroke-linecap="round"');
 [[385,143,'+2'],[382,251,'+1'],[382,445,'−1'],[385,555,'−2'],[146,378,'−2'],[261,378,'−1'],[459,378,'+1'],[576,378,'+2']].forEach(([x,y,label])=>{a+=t(x,y,label,F.typography.sizes.small,'muted');});
 if(v){const x=(360+lat*radius/2).toFixed(2),y=(360-lon*radius/2).toFixed(2);a+='<g data-field="gforce" data-state="'+(alert?'alert':'normal')+'">';
  if(alert)a+='<circle data-g-limit="configured" cx="360" cy="360" r="'+(m.limits.gforce*radius/2)+'" fill="none" stroke="'+C.amber+'" stroke-width="2" stroke-dasharray="6 8"/>';
  a+='<circle cx="'+x+'" cy="'+y+'" r="16" fill="none" stroke="'+C.amber+'" stroke-width="2"/><circle data-g-meter-point="current" data-lat="'+lat+'" data-lon="'+lon+'" aria-label="横向 '+lat.toFixed(2)+' g，纵向 '+lon.toFixed(2)+' g" cx="'+x+'" cy="'+y+'" r="10" fill="'+C[alert?'amber':'text']+'"/>';
  a+='</g>';
 }break;}
case 'heart':{
 const alert=find('heart');
 a+=header('DRIVER PULSE',!v?'SENSOR / WAITING':alert?'RANGE '+m.limits.heartLow+' / '+m.limits.heartHigh+' bpm':'LIVE / HEART RATE')+reading('heart',200,190,'PULSE',n.heart,'bpm');
 if(v)a+=trend([20,22,21,22,30,22,16,55,4,22,24,21,22,29,21,16,58,4,22,22,21],115,440,490,65,'amber').replace('<path ','<path data-live-indicator="heart-trend" ');
 else a+=path('M115 478H605','line',2);
 a+=path('M112 532H608');
 [[168,'LOW',n.low],[360,'PEAK',n.peak],[552,'1 MIN',n.average]].forEach(([x,label,value])=>{const color=!v?'muted':alert?(value<m.limits.heartLow||value>m.limits.heartHigh?'amber':'text'):label==='PEAK'?'amber':'text';a+=t(x,550,label,F.typography.sizes.small,'muted')+t(x,582,v?value:dash,F.typography.sizes.statistic,color,'middle',F.typography.number);});
 if(alert)a+=alertMessage(m.alerts,640);break;}
case 'analog':{
 const pressure=n.boost,angle=135+(pressure+1)/4*270;
 for(let i=0;i<=40;i++){
  const d=135+i*270/40,major=i%10===0,mid=i%5===0,p=pt(360,360,280,d),q=pt(360,360,major?250:mid?260:269,d);
  a+=path('M'+p.join(' ')+'L'+q.join(' '),i>=35?'red':mid?'text':'line',major?5:mid?3:2,'data-analog-tick="'+i+'"');
 }
 [-1,0,1,2,3].forEach(value=>{const p=pt(360,360,218,135+(value+1)/4*270);a+=t(+p[0],+p[1]-14,value,F.typography.sizes.label,'muted');});
 a+=t(360,186,'BOOST',F.typography.sizes.title,'amber');
 if(v)a+=needle(360,360,220,angle,ink('boost','amber')).replace('data-live-indicator="needle"','data-live-indicator="needle" data-analog-needle="current"');
 else a+='<circle cx="360" cy="360" r="9" fill="'+C.line+'"/>';
 a+=alertField('boost',t(360,466,v?pressure.toFixed(2):dash,F.typography.sizes.analog,ink('boost'),'middle',F.typography.number)+t(360,579,'bar',F.typography.sizes.support,'muted'),find('boost'))+alertMessage(m.alerts);break;}
case 'music':{
 const song=songs[s.track],available=m.mediaAvailable,secs=Math.round(s.progress),time=n=>Math.floor(n/60)+':'+String(n%60).padStart(2,'0');
 a+=header('NOW PLAYING',available?'ROADSTER RADIO':'MEDIA / UNAVAILABLE');
 a+='<circle cx="174" cy="284" r="68" fill="'+C.panel+'"/><circle cx="174" cy="284" r="52" fill="none" stroke="'+C.line+'" stroke-width="2"/><circle cx="174" cy="284" r="35" fill="none" stroke="'+C.line+'" stroke-width="2"/><circle cx="174" cy="284" r="12" fill="'+C[available?'amber':'muted']+'"/>';
 a+=t(280,233,available?song.title:'No media',F.typography.sizes.mediaTitle,'text','start',F.typography.number,600)+t(280,305,available?song.artist:'SOURCE UNAVAILABLE',F.typography.sizes.small,'muted','start');
 if(available){
  a+=t(360,355,song.previous,F.typography.sizes.support,'muted','middle',F.typography.body,400)+t(360,399,song.lyric,F.typography.sizes.body,'text','middle',F.typography.body,500)+t(360,443,song.next,F.typography.sizes.support,'muted','middle',F.typography.body,400);
 }else a+=t(360,399,'连接媒体设备后继续播放',F.typography.sizes.body,'muted','middle',F.typography.body,400);
 a+=r(150,480,420,6,'panel',3)+(available?r(150,480,420*secs/song.duration,6,'amber',3):'')+t(150,498,available?time(secs):'—:—',F.typography.sizes.small,'muted','start')+t(570,498,available?time(song.duration):'—:—',F.typography.sizes.small,'muted','end');
 a+=mediaButton(200,540,96,'上一首','prev-track','previous','panel',!available)+mediaButton(312,540,96,available&&s.playing?'暂停':'播放','play',available&&s.playing?'pause':'play','panel',!available)+mediaButton(424,540,96,'下一首','next-track','next','panel',!available);break;}
case 'display':
 a+=header('ILLUMINATION','DAY / NIGHT MEMORY');
 a+=t(146,188,'DAY',F.typography.sizes.title,'text','start')+t(574,188,s.day+'%',F.typography.sizes.title,'amber','end')+r(146,242,428,8,'panel',4)+r(146,242,428*s.day/100,8,'amber',4)+roundButton(146,264,96,'−','day-minus',s.day<=10)+roundButton(478,264,96,'+','day-plus',s.day>=100);
 a+=t(146,368,'NIGHT',F.typography.sizes.title,'text','start')+t(574,368,s.dark+'%',F.typography.sizes.title,'amber','end')+r(146,422,428,8,'panel',4)+r(146,422,428*s.dark/100,8,'amber',4)+roundButton(146,444,96,'−','night-minus',s.dark<=10)+roundButton(478,444,96,'+','night-plus',s.dark>=100);
 a+=btn(240,552,240,s.night?'NIGHT ACTIVE':'DAY ACTIVE','mode','amber');break;
case 'bluetooth':{
 const connected=m.paired&&!m.connectionLost;
 a+=header('CONNECTIONS','BLUETOOTH');
 a+=chip(175,176,370,m.connectionLost?'CONNECTION LOST':m.paired?'DEVICE CONNECTED':'NO DEVICE',m.connectionLost?'amber':'panel')+t(360,232,m.connectionLost?'RECONNECT YOUR PHONE':m.paired?'ROADSTER PHONE':'PAIR A NEW DEVICE',F.typography.sizes.label,m.connectionLost?'amber':connected?'green':'muted')+path('M128 270H592');
 a+=t(142,318,'DISCOVERABLE',F.typography.sizes.label,'text','start')+toggleControl(474,286,104,'蓝牙可发现','visible',s.visible);
 a+=t(142,418,'REMOTE KEY',F.typography.sizes.label,'text','start')+toggleControl(474,386,104,'遥控键盘','keyboard',s.keyboard);
 a+=btn(160,502,112,'BACK','back')+btn(292,502,112,'PAIR','pair','amber')+btn(424,502,136,'FORGET\nALL','forget');
 if(m.connectionLost)a+=t(360,615,'SAVED PAIRING RETAINED',F.typography.sizes.small,'muted');break;}
case 'pair':{
 const failed=m.pairingFailed,expired=m.scenario==='pair-expired';
 a+=header('PAIR DEVICE','BLUETOOTH')+icon(332,176,'M15 0V72L44 51L0 15M0 57L44 21L15 0','blue',1.1);
 a+=t(360,278,failed?expired?'CODE EXPIRED':'PAIRING FAILED':'PAIRING CODE',F.typography.sizes.support,failed?'amber':'muted')+t(360,318,expired?'— —':s.pairCode||'1234 5678',F.typography.sizes.pair,failed?'muted':'text','middle',F.typography.number,600);
 a+=t(360,402,failed?expired?'Generate a new code':'Check phone and try again':'Enter this code on your phone',F.typography.sizes.small,failed?'amber':'muted');
 a+=btn(190,440,340,failed?expired?'NEW PAIRING CODE':'RETRY PAIRING':'CONFIRM PAIRING',failed?'retry-pair':'confirm-pair','amber')+btn(248,552,224,'BACK','connections');break;}
}
a+=alertRim(m.alerts);
if(s.dialog&&id==='bluetooth'){
a='<g aria-hidden="true" pointer-events="none">'+a.replaceAll('tabindex="0"','tabindex="-1"')+'</g>';
a+='<circle cx="360" cy="360" r="360" fill="'+C.face+'" opacity=".94"/><g role="dialog" aria-label="Clear paired devices">'+r(114,200,492,354,'panel',20)+t(360,240,'CLEAR PAIRED DEVICES?',F.typography.sizes.body,'red')+t(360,299,'PHONE + REMOTE KEYBOARD',F.typography.sizes.small,'muted')+t(360,335,'Saved pairings will be',F.typography.sizes.small,'text')+t(360,367,'removed from this device.',F.typography.sizes.small,'text')+btn(148,434,192,'CANCEL','cancel')+btn(380,434,192,'CLEAR','clear','red')+'</g>';
}
if(s.night)a='<g class="night-colors">'+a.replaceAll(C.text,C.night)+'</g>';
return '<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720" role="group" aria-label="'+esc(PAGES.find(p=>p.id===id)?.title||id)+'"><defs><clipPath id="round"><circle cx="360" cy="360" r="360"/></clipPath></defs><g clip-path="url(#round)">'+a+'</g></svg>';
}
root.NC={pages:PAGES,defaults,songs,get state(){return state;},render,colors:C,scenarios:S.pages,resolve:S.resolve};
if(typeof document==='undefined')return;
const app=document.getElementById('nc-app'),face=document.getElementById('active-dial'),gallery=document.getElementById('gallery'),nav=document.getElementById('page-nav');
if(!app)return;
const requestedPage=new URLSearchParams(window.location.search).get('page');
if(PAGES.some(p=>p.id===requestedPage))state.page=requestedPage;
const requestedScenario=new URLSearchParams(window.location.search).get('scenario');
if(S.pages[state.page].some(entry=>entry.id===requestedScenario))state.scenarios[state.page]=requestedScenario;
document.getElementById('speed-preview-slider').max=F.road.maximumPreviewSpeed;
let roadDistance=0,roadFrame=0,roadTime=null,roadItems=[],roadDrive=roadMotion(0,false);
const reducedRoadMotion=window.matchMedia('(prefers-reduced-motion: reduce)');
function animateRoad(time){
 roadFrame=0;
 const motion=roadDrive;
 if(state.page!=='speed'||!motion.moving||document.hidden||reducedRoadMotion.matches){roadTime=null;return;}
 if(roadTime!==null)roadDistance=(roadDistance+motion.metersPerSecond*Math.min(100,time-roadTime)/1000)%(F.road.spacingMeters*F.road.objectsPerSide);
 roadTime=time;
 for(const item of roadItems){
  const phase=item.slot/F.road.objectsPerSide+(item.side>0?.11:0)+roadDistance/(F.road.spacingMeters*F.road.objectsPerSide),p=roadProjection(item.side,phase);
  item.node.setAttribute('transform',p.transform);item.node.setAttribute('opacity',p.opacity.toFixed(3));
 }
 roadFrame=window.requestAnimationFrame(animateRoad);
}
function syncRoadMotion(){
 roadItems=Array.from(face.querySelectorAll('.roadside-object')).map(node=>({node,side:Number(node.dataset.roadSide),slot:Number(node.dataset.roadSlot)}));
 const motion=roadDrive=roadMotion(S.resolve('speed',state).values.speed,state.signal);
 const active=state.page==='speed'&&motion.moving&&!document.hidden&&!reducedRoadMotion.matches;
 if(!active){window.cancelAnimationFrame(roadFrame);roadFrame=0;roadTime=null;return;}
 if(!roadFrame){roadTime=null;roadFrame=window.requestAnimationFrame(animateRoad);}
}
document.addEventListener('visibilitychange',syncRoadMotion);
reducedRoadMotion.addEventListener('change',syncRoadMotion);
function persist(){try{localStorage.setItem('nc-prototype-state',JSON.stringify(state));}catch{}}
function show(){
face.innerHTML=render(state.page,{...state,roadDistance});
syncRoadMotion();
face.style.filter=state.night?'brightness('+(0.48+state.dark*.0052)+')':'brightness('+(0.6+state.day*.004)+')';
const p=PAGES.find(p=>p.id===state.page);document.getElementById('active-title').textContent=p.title;document.getElementById('active-desc').textContent=p.desc;
document.querySelectorAll('[data-page]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.page===state.page)));
document.getElementById('mode-switch').textContent=state.night?'夜间预览':'日间预览';
const model=S.resolve(state.page,state),entries=S.pages[state.page],scene=S.selected(state.page,state);
const alarmSwitch=document.getElementById('alarm-switch'),signalSwitch=document.getElementById('signal-switch');
alarmSwitch.setAttribute('aria-pressed',String(scene.id!=='normal'));
alarmSwitch.disabled=entries.length===1;alarmSwitch.textContent=scene.id==='normal'?'模拟异常':'恢复正常';
signalSwitch.disabled=!model.vehicle&&state.page!=='heart';
signalSwitch.textContent=signalSwitch.disabled?'独立系统状态':model.connected?'数据正常':'模拟断连';
const select=document.getElementById('scenario-select');
select.innerHTML=entries.map(entry=>'<option value="'+entry.id+'">'+esc(entry.label)+'</option>').join('');select.value=scene.id;select.disabled=entries.length===1;
document.getElementById('scenario-description').textContent=!model.connected?'无数据：已隐藏有效指示，越限提醒暂不显示。':state.page==='gforce'?(model.alerts.length?'当前 G 值超过所设提醒阈值；虚线圈表示阈值。':'正常显示当前点；G 值提醒默认关闭。'):model.alerts.length?model.alerts.map(alert=>alert.reason).join(' · '):scene.id!=='normal'?({music:'媒体不可用：保留页面，播放控件禁用。',bluetooth:'连接与配对状态独立于车辆数据。',pair:'保留返回操作，并提供重试。'})[state.page]||scene.label:'正常状态；限值提醒默认关闭，场景数值仅用于设计演示。';
document.getElementById('wheel-options').hidden=state.page!=='tyres'||['normal','multiple'].includes(scene.id);
document.getElementById('wheel-select').value=state.tyreWheel;
document.getElementById('speed-preview').hidden=state.page!=='speed';
document.getElementById('speed-preview-slider').value=model.values.speed;
document.getElementById('speed-preview-slider').disabled=state.page==='speed'&&!model.connected;
document.getElementById('speed-preview-value').textContent=model.values.speed+' km/h';
document.getElementById('limit-options').hidden=!['speed','gforce','heart'].includes(state.page);
document.querySelectorAll('[data-limit-page]').forEach(el=>{el.hidden=el.dataset.limitPage!==state.page;});
document.querySelectorAll('[data-limit]').forEach(el=>{el.value=state.limits[el.dataset.limit];});
document.getElementById('seek').disabled=!S.resolve('music',state).mediaAvailable;
persist();
}
nav.innerHTML=PAGES.map(p=>'<button data-page="'+p.id+'" type="button">'+esc(p.title)+'</button>').join('');
gallery.innerHTML=PAGES.map((p,i)=>'<button class="gallery-item" data-page="'+p.id+'" type="button" aria-label="查看'+p.title+'"><span class="gallery-label"><span>'+String(i+1).padStart(2,'0')+'</span> '+esc(p.title)+'</span><span class="gallery-face" aria-hidden="true">'+render(p.id,defaults).replaceAll('tabindex="0"','tabindex="-1"')+'</span><span class="gallery-caption">'+esc(p.english)+'</span></button>').join('');
app.addEventListener('click',e=>{
const page=e.target.closest('[data-page]');if(page){state.page=page.dataset.page;state.dialog=false;show();document.getElementById('live-preview').scrollIntoView({behavior:'smooth',block:'start'});return;}
const el=e.target.closest('[data-act]');if(!el||el.getAttribute('aria-disabled')==='true')return;action(el.dataset.act);
});
app.addEventListener('keydown',e=>{if((e.key==='Enter'||e.key===' ')&&e.target.matches('.dial-control')&&e.target.getAttribute('aria-disabled')!=='true'){e.preventDefault();action(e.target.dataset.act);}});
function action(act){
switch(act){
case 'mode':state.night=!state.night;break;
case 'alarm':state.scenarios[state.page]=S.selected(state.page,state).id==='normal'?(S.pages[state.page][1]?.id||'normal'):'normal';if(state.page==='speed')state.previewSpeed=null;break;
case 'signal':
 if(state.page==='heart'){
  state.heartSignal=!S.resolve('heart',state).connected;
  if(state.heartSignal&&state.scenarios.heart==='no-data')state.scenarios.heart='normal';
 }else state.signal=!state.signal;
 break;
case 'day-plus':state.day=Math.min(100,state.day+5);break;
case 'day-minus':state.day=Math.max(10,state.day-5);break;
case 'night-plus':state.dark=Math.min(100,state.dark+5);break;
case 'night-minus':state.dark=Math.max(10,state.dark-5);break;
case 'visible':state.visible=!state.visible;break;
case 'keyboard':state.keyboard=!state.keyboard;break;
case 'pair':state.page='pair';break;
case 'connections':state.page='bluetooth';break;
case 'back':state.page='gear';break;
case 'forget':state.dialog=true;break;
case 'cancel':state.dialog=false;break;
case 'clear':state.paired=false;state.keyboard=false;state.dialog=false;state.scenarios.bluetooth='normal';break;
case 'confirm-pair':state.paired=true;state.scenarios.bluetooth='normal';state.page='bluetooth';break;
case 'retry-pair':
 if(S.selected('pair',state).id==='pair-expired')state.pairCode=state.pairCode==='1234 5678'?'2468 1357':'1234 5678';
 state.scenarios.pair='normal';break;
case 'play':state.playing=!state.playing;break;
case 'prev-track':state.track=(state.track+songs.length-1)%songs.length;state.progress=0;break;
case 'next-track':state.track=(state.track+1)%songs.length;state.progress=0;break;
case 'channel':state.channel=state.channel==='TPS'?'B.P':'TPS';break;
case 'actual-size':app.classList.toggle('actual-size');break;
}
show();
}
document.getElementById('seek').addEventListener('input',e=>{state.progress=Number(e.target.value)*songs[state.track].duration/100;show();});
document.getElementById('day-slider').addEventListener('input',e=>{state.day=Number(e.target.value);show();});
document.getElementById('night-slider').addEventListener('input',e=>{state.dark=Number(e.target.value);show();});
document.getElementById('scenario-select').addEventListener('change',e=>{state.scenarios[state.page]=e.target.value;if(state.page==='speed')state.previewSpeed=null;show();});
document.getElementById('wheel-select').addEventListener('change',e=>{state.tyreWheel=e.target.value;show();});
document.getElementById('speed-preview-slider').addEventListener('input',e=>{state.previewSpeed=Number(e.target.value);show();});
document.querySelectorAll('[data-limit]').forEach(input=>{
 input.addEventListener('input',e=>{
  if(e.target.value===''||!e.target.checkValidity())return;
  state.limits[e.target.dataset.limit]=Number(e.target.value);show();
 });
 const restoreInvalid=e=>{
  if(e.target.value===''||!e.target.checkValidity())e.target.value=state.limits[e.target.dataset.limit];
 };
 input.addEventListener('change',restoreInvalid);input.addEventListener('blur',restoreInvalid);
});
show();
})(typeof window!=='undefined'?window:globalThis);
