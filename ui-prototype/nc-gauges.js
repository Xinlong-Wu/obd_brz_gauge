(function(root){
'use strict';
const F=root.NC_FOUNDATIONS,C=F.colors;
const PAGES=[
{id:'gear',group:'车辆仪表',title:'挡位 / 转速',english:'GEAR & RPM',desc:'大挡位与分段转速弧；温度和油压位于下方。'},
{id:'boost',group:'车辆仪表',title:'增压 / 真空',english:'MANIFOLD',desc:'270°压力弧、细指针与偏下方读数，替代斜坡面积图。'},
{id:'coolant',group:'车辆仪表',title:'冷却液温度',english:'COOLANT',desc:'纵向温度柱、数字和状态文字，替代横向尺标。'},
{id:'oiltemp',group:'车辆仪表',title:'发动机油温',english:'OIL TEMPERATURE',desc:'温度弧与主读数居中对齐，保留暖机与报警状态。'},
{id:'oilpress',group:'车辆仪表',title:'发动机油压',english:'OIL PRESSURE',desc:'左侧偏心机械表与右侧读数区各自留出空间；压力、单位和状态共用右侧对齐轴，圆弧与数字保持间距。'},
{id:'triple',group:'车辆仪表',title:'三项综合监测',english:'POWERTRAIN',desc:'主增压读数与下方油温、油压双分区。'},
{id:'multi',group:'车辆仪表',title:'多项综合监测',english:'ENGINE VITALS',desc:'中间右侧以 TPS / B.P 标签和大读数组成整体切换区；点击标签、数字或留白均切换指标，琥珀色和下划线标明当前项。'},
{id:'tyres',group:'车辆仪表',title:'四轮胎压 / 胎温',english:'TYRE MONITOR',desc:'原创敞篷跑车俯视线稿，四轮定位；文字与颜色共同提示异常。'},
{id:'speed',group:'动态与健康',title:'车速 / 巡航',english:'SPEED',desc:'路面透视构图，当前车速为主、巡航设定为辅。'},
{id:'gforce',group:'动态与健康',title:'G 力 / 当前点',english:'G-FORCE',desc:'同心圆与十字刻度：内圈 1g、外圈 2g，只显示当前坐标点和双轴读数。'},
{id:'heart',group:'动态与健康',title:'心率 / 趋势',english:'DRIVER PULSE',desc:'实时心率、低值、峰值、1分钟统计与脉搏波形。'},
{id:'analog',group:'车辆仪表',title:'机械压力表',english:'PRESSURE / ANALOG',desc:'参考 01／02／04 的表盘构图：居中的 270°针式刻度，BOOST 小标识融入表盘内部，下方直接显示压力读数与单位。'},
{id:'music',group:'媒体与系统',title:'音乐 / 歌词',english:'NOW PLAYING',desc:'圆形主播放键配轻量切歌图标，保留唱片、歌词与进度信息。'},
{id:'display',group:'媒体与系统',title:'日间 / 夜间亮度',english:'ILLUMINATION',desc:'圆形加减键与精密模式键，分别调节日间和夜间亮度。'},
{id:'bluetooth',group:'媒体与系统',title:'蓝牙 / 遥控',english:'CONNECTIONS',desc:'拨片式开关配轻量返回与配对操作，保留设备状态和配对管理。'},
{id:'pair',group:'媒体与系统',title:'配对码',english:'PAIR DEVICE',desc:'分组配对码、配对确认和返回导航。'}
];
const defaults={page:'gear',night:false,day:85,dark:35,alarm:false,signal:true,visible:true,keyboard:true,paired:true,playing:true,track:0,progress:69,dialog:false,channel:'TPS'};
const songs=[{title:'Night Drive',artist:'ROADSTER RADIO',lyric:'沿着夜色，驶向下一个弯道',previous:'把城市留在身后',next:'让风穿过自由的方向',duration:189},{title:'Open Sky',artist:'ROADSTER RADIO',lyric:'抬头是星光，前方是远方',previous:'驶过漫长的街灯',next:'把时间交给这条路',duration:205}];
let state=Object.assign({},defaults);
const {esc,t,r,path,pt,arc,art,metric,header,shell,chip,btn,controlHit,roundButton,toggleControl,readoutControl,icon,mediaButton,trend,readout,statusText}=root.NC_COMPONENTS;
function render(id,s=state){
const v=s.signal,noSignal=!v&&!['music','display','bluetooth','pair','heart'].includes(id);
let a=shell(noSignal?'NO SIGNAL':'');const dash='—'; const warn=s.alarm;
switch(id){
case 'gear':
a+=art('tach')+t(360,172,'MX-5 / NC',F.typography.sizes.label,'amber')+metric(200,240,'GEAR',v?'4':dash,v?'4,800 RPM':'RPM')+t(360,535,v?'OIL 104°C    PRESS 3.8 bar':'WAITING FOR VEHICLE',F.typography.sizes.support,'text');
if(warn)a+=chip(230,580,260,'SHIFT / 7,200','red');break;
case 'boost':
a+=art('boost')+t(360,178,'MANIFOLD',F.typography.sizes.title,'amber')+t(360,325,'VACUUM / BOOST',F.typography.sizes.small,'muted')+metric(200,375,'BOOST',v?'0.68':dash,'bar');break;
case 'coolant':
a+=header('COOLANT','ENGINE THERMAL')+art('thermal')+metric(265,220,'WATER',v?(warn?'112':'92'):dash,'°C',false,warn?'red':'text')+t(420,530,warn?'TEMP HIGH':'STABLE',F.typography.sizes.label,warn?'red':'green')+t(112,208,'120',F.typography.sizes.support,'muted')+t(112,480,'60',F.typography.sizes.support,'muted');break;
case 'oiltemp':{
const oilValue=warn?130:104;
a+=art('oil');
if(v)a+=path(arc(360,360,246,135,135+Math.min(1,Math.max(0,(oilValue-40)/100))*270),warn?'red':'amber',6);
a+=t(360,172,'OIL TEMPERATURE',F.typography.sizes.title,'amber')+metric(200,242,'ENGINE OIL',v?String(oilValue):dash,'°C',false,warn?'red':'text')+t(360,540,v?(warn?'OIL TEMP HIGH':'WARM / STABLE'):'WAITING',F.typography.sizes.support,v?(warn?'red':'green'):'muted');break;}
case 'oilpress':
a+=header('OIL PRESSURE','LUBRICATION')+'<g data-oilpress-dial="eccentric">'+art('press')+'</g>';
a+='<g data-oilpress-readout="numeric">'+readout({x:360,y:240,label:'PRESSURE',value:warn?'0.5':'3.8',unit:'bar',size:'eccentric',state:!v?'disconnected':warn?'warning':'normal'})+statusText(520,540,v?(warn?'LOW PRESSURE':'STABLE'):'WAITING',!v?'disconnected':warn?'warning':'normal')+'</g>';break;
case 'triple':
a+=header('POWERTRAIN','THREE CHANNEL VIEW')+art('triple')+metric(200,215,'BOOST',v?'0.68':dash,'bar')+metric(130,450,'OIL TEMP',v?'104':dash,'°C',true)+metric(370,450,'OIL PRESS',v?(warn?'0.5':'3.8'):dash,'bar',true,warn?'red':'text');break;
case 'multi':
a+=header('ENGINE VITALS')+path('M108 314H612M360 162V308M108 466H612M360 470V610')+metric(115,166,'OIL PRESS',v?(warn?'0.5':'3.8'):dash,'bar',true,warn?'red':'text')+metric(375,166,'OIL TEMP',v?'104':dash,'°C',true)+metric(115,477,'BATTERY',v?'14.2':dash,'V',true)+metric(375,477,'COOLANT',v?'92':dash,'°C',true);
a+=t(206,336,v?'0.68':dash,F.typography.sizes.boost,'amber','middle',F.typography.number)+t(206,427,'BOOST / bar',F.typography.sizes.small,'muted')+readoutControl(s.channel==='TPS'?'TPS':'B.P',v?(s.channel==='TPS'?'38%':'21%'):dash);break;
case 'tyres':
a+=header('TYRE MONITOR','MX-5 / ROADSTER')+art('roadster');
[[186,252,'FL','2.2','35'],[534,252,'FR','2.2','35'],[186,430,'RL','2.1','37'],[534,430,'RR',warn?'0.5':'2.1','37']].forEach(q=>{a+=t(q[0],q[1],q[2],F.typography.sizes.small,'muted')+t(q[0],q[1]+29,v?q[3]:dash,F.typography.sizes.tyre,warn&&q[2]==='RR'?'red':'text','middle',F.typography.number)+t(q[0],q[1]+112,'bar / '+q[4]+'°C',F.typography.sizes.small,'muted');});
if(warn)a+=t(360,592,'RR / LOW PRESSURE',F.typography.sizes.support,'red');break;
case 'speed':
a+=header('SPEED','ROADSTER / TOURING')+path('M158 575L292 380M562 575L428 380','line',3)+path('M360 550V524M360 490V474','amber',5)+metric(200,204,'VELOCITY',v?'105':dash,'km/h')+chip(224,490,272,'CRUISE ACTIVE','panel')+t(360,551,'SET 100 km/h',F.typography.sizes.title,'green');break;
case 'gforce':{
const radius=238,lat=0.52,lon=0.18;
a+=header('G-FORCE');
a+='<circle data-g-meter-ring="2" cx="360" cy="360" r="'+radius+'" fill="none" stroke="'+C.amber+'" stroke-width="3"/><circle data-g-meter-ring="1" cx="360" cy="360" r="'+(radius/2)+'" fill="none" stroke="'+C.line+'" stroke-width="2"/>';
a+=path('M122 360H598M360 122V598','line',2);
a+=path('M122 360H152M568 360H598M360 122V152M360 568V598','amber',6,'stroke-linecap="round"');
a+=path('M241 360H258M462 360H479M360 241V258M360 462V479','amber',4,'stroke-linecap="round"');
a+=t(385,143,'+2',F.typography.sizes.small,'muted')+t(382,251,'+1',F.typography.sizes.small,'muted')+t(382,445,'−1',F.typography.sizes.small,'muted')+t(385,555,'−2',F.typography.sizes.small,'muted');
a+=t(146,378,'−2',F.typography.sizes.small,'muted')+t(261,378,'−1',F.typography.sizes.small,'muted')+t(459,378,'+1',F.typography.sizes.small,'muted')+t(576,378,'+2',F.typography.sizes.small,'muted');
if(v){const x=(360+lat*radius/2).toFixed(2),y=(360-lon*radius/2).toFixed(2);
a+='<circle cx="'+x+'" cy="'+y+'" r="16" fill="none" stroke="'+C.amber+'" stroke-width="2"/><circle data-g-meter-point="current" data-lat="'+lat+'" data-lon="'+lon+'" cx="'+x+'" cy="'+y+'" r="10" fill="'+C.text+'"/>';
a+=t(360,610,'LAT +0.52  /  LON +0.18 g',F.typography.sizes.support,'text');}break;}
case 'heart':
a+=header('DRIVER PULSE','LIVE / HEART RATE')+metric(200,190,'PULSE','105','bpm');
a+=trend([20,22,21,22,30,22,16,55,4,22,24,21,22,29,21,16,58,4,22,22,21],115,440,490,65,'amber');
a+=path('M112 532H608')+t(168,550,'LOW',F.typography.sizes.small,'muted')+t(360,550,'PEAK',F.typography.sizes.small,'muted')+t(552,550,'1 MIN',F.typography.sizes.small,'muted')+t(168,582,'60',F.typography.sizes.statistic,'text','middle',F.typography.number)+t(360,582,'165',F.typography.sizes.statistic,'amber','middle',F.typography.number)+t(552,582,'105',F.typography.sizes.statistic,'text','middle',F.typography.number);break;
case 'analog':{
const pressure=0.68,angle=135+(pressure+1)/4*270;
for(let i=0;i<=40;i++){
const d=135+i*270/40,major=i%10===0,mid=i%5===0,p=pt(360,360,280,d),q=pt(360,360,major?250:mid?260:269,d);
a+=path('M'+p.join(' ')+'L'+q.join(' '),i>=35?'red':mid?'text':'line',major?5:mid?3:2,'data-analog-tick="'+i+'"');
}
[-1,0,1,2,3].forEach(value=>{const p=pt(360,360,218,135+(value+1)/4*270);a+=t(+p[0],+p[1]-14,value,F.typography.sizes.label,'muted');});
a+=t(360,186,'BOOST',F.typography.sizes.title,'amber');
if(v)a+=path('M360 360L'+pt(360,360,220,angle).join(' '),'amber',5,'data-analog-needle="current"');
a+='<circle cx="360" cy="360" r="11" fill="'+C[v?'amber':'line']+'"/>'+t(360,466,v?pressure.toFixed(2):dash,F.typography.sizes.analog,'text','middle',F.typography.number)+t(360,579,'bar',F.typography.sizes.support,'muted');break;}
case 'music':{
const song=songs[s.track],secs=Math.round(s.progress),time=n=>Math.floor(n/60)+':'+String(n%60).padStart(2,'0');
a+=header('NOW PLAYING','ROADSTER RADIO');
a+='<circle cx="174" cy="284" r="68" fill="'+C.panel+'"/><circle cx="174" cy="284" r="52" fill="none" stroke="'+C.line+'" stroke-width="2"/><circle cx="174" cy="284" r="35" fill="none" stroke="'+C.line+'" stroke-width="2"/><circle cx="174" cy="284" r="12" fill="'+C.amber+'"/>';
a+=t(280,233,song.title,60,'text','start',F.typography.number,600)+t(280,305,song.artist,F.typography.sizes.small,'muted','start');
a+=t(360,355,song.previous,F.typography.sizes.support,'muted','middle',F.typography.body,400)+t(360,399,song.lyric,F.typography.sizes.body,'text','middle',F.typography.body,500)+t(360,443,song.next,F.typography.sizes.support,'muted','middle',F.typography.body,400);
a+=r(150,480,420,6,'panel',3)+r(150,480,420*secs/song.duration,6,'amber',3)+t(150,498,time(secs),F.typography.sizes.small,'muted','start')+t(570,498,time(song.duration),F.typography.sizes.small,'muted','end');
a+=mediaButton(200,540,96,'上一首','prev-track','previous')+mediaButton(312,540,96,s.playing?'暂停':'播放','play',s.playing?'pause':'play')+mediaButton(424,540,96,'下一首','next-track','next');break;}
case 'display':
a+=header('ILLUMINATION','DAY / NIGHT MEMORY');
a+=t(146,188,'DAY',F.typography.sizes.title,'text','start')+t(574,188,s.day+'%',F.typography.sizes.title,'amber','end')+r(146,242,428,8,'panel',4)+r(146,242,428*s.day/100,8,'amber',4)+roundButton(146,264,96,'−','day-minus',s.day<=10)+roundButton(478,264,96,'+','day-plus',s.day>=100);
a+=t(146,368,'NIGHT',F.typography.sizes.title,'text','start')+t(574,368,s.dark+'%',F.typography.sizes.title,'amber','end')+r(146,422,428,8,'panel',4)+r(146,422,428*s.dark/100,8,'amber',4)+roundButton(146,444,96,'−','night-minus',s.dark<=10)+roundButton(478,444,96,'+','night-plus',s.dark>=100);
a+=btn(240,552,240,s.night?'NIGHT ACTIVE':'DAY ACTIVE','mode','amber');break;
case 'bluetooth':
a+=header('CONNECTIONS','BLUETOOTH');
a+=chip(175,176,370,s.paired?'DEVICE CONNECTED':'NO DEVICE','panel')+t(360,232,s.paired?'ROADSTER PHONE':'PAIR A NEW DEVICE',F.typography.sizes.label,s.paired?'green':'muted')+path('M128 270H592');
a+=t(142,318,'DISCOVERABLE',F.typography.sizes.label,'text','start')+toggleControl(474,286,104,'蓝牙可发现','visible',s.visible);
a+=t(142,418,'REMOTE KEY',F.typography.sizes.label,'text','start')+toggleControl(474,386,104,'遥控键盘','keyboard',s.keyboard);
a+=btn(160,502,112,'BACK','back')+btn(292,502,112,'PAIR','pair','amber')+btn(424,502,136,'FORGET\nALL','forget');break;
case 'pair':
a+=header('PAIR DEVICE','BLUETOOTH');
a+=icon(332,176,'M15 0V72L44 51L0 15M0 57L44 21L15 0', 'blue',1.1)+t(360,278,'PAIRING CODE',F.typography.sizes.support,'muted')+t(360,318,'1234 5678',F.typography.sizes.pair,'text','middle',F.typography.number,600)+t(360,402,'Enter this code on your phone',F.typography.sizes.small,'muted')+btn(190,440,340,'CONFIRM PAIRING','confirm-pair','amber')+btn(248,552,224,'BACK','connections');break;
}
if(s.dialog&&id==='bluetooth'){
a='<g aria-hidden="true" pointer-events="none">'+a.replaceAll('tabindex="0"','tabindex="-1"')+'</g>';
a+='<circle cx="360" cy="360" r="360" fill="'+C.face+'" opacity=".94"/><g role="dialog" aria-label="Clear paired devices">'+r(114,200,492,354,'panel',20)+t(360,240,'CLEAR PAIRED DEVICES?',F.typography.sizes.body,'red')+t(360,299,'PHONE + REMOTE KEYBOARD',F.typography.sizes.small,'muted')+t(360,335,'Saved pairings will be',F.typography.sizes.small,'text')+t(360,367,'removed from this device.',F.typography.sizes.small,'text')+btn(148,434,192,'CANCEL','cancel')+btn(380,434,192,'CLEAR','clear','red')+'</g>';
}
if(s.night)a='<g class="night-colors">'+a.replaceAll(C.text,C.night)+'</g>';
return '<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720" role="group" aria-label="'+esc(PAGES.find(p=>p.id===id)?.title||id)+'"><defs><clipPath id="round"><circle cx="360" cy="360" r="360"/></clipPath></defs><g clip-path="url(#round)">'+a+'</g></svg>';
}
root.NC={pages:PAGES,defaults,songs,get state(){return state;},render,colors:C};
if(typeof document==='undefined')return;
const app=document.getElementById('nc-app'),face=document.getElementById('active-dial'),gallery=document.getElementById('gallery'),nav=document.getElementById('page-nav');
if(!app)return;
const requestedPage=new URLSearchParams(window.location.search).get('page');
if(PAGES.some(p=>p.id===requestedPage))state.page=requestedPage;
function persist(){try{localStorage.setItem('nc-prototype-state',JSON.stringify(state));}catch{}}
function show(){
face.innerHTML=render(state.page);
face.style.filter=state.night?'brightness('+(0.48+state.dark*.0052)+')':'brightness('+(0.6+state.day*.004)+')';
const p=PAGES.find(p=>p.id===state.page);document.getElementById('active-title').textContent=p.title;document.getElementById('active-desc').textContent=p.desc;
document.querySelectorAll('[data-page]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.page===state.page)));
document.getElementById('mode-switch').textContent=state.night?'夜间预览':'日间预览';
document.getElementById('alarm-switch').setAttribute('aria-pressed',String(state.alarm));
document.getElementById('signal-switch').textContent=state.signal?'数据正常':'模拟断连';
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
case 'alarm':state.alarm=!state.alarm;break;
case 'signal':state.signal=!state.signal;break;
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
case 'clear':state.paired=false;state.keyboard=false;state.dialog=false;break;
case 'confirm-pair':state.paired=true;state.page='bluetooth';break;
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
show();
})(typeof window!=='undefined'?window:globalThis);
