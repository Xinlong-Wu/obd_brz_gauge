(function(root){
'use strict';
const F=root.NC_FOUNDATIONS,U=root.NC_COMPONENTS,C=F.colors;
const app=document.getElementById('foundation-app');
if(!app)return;
let readoutState='normal',playing=true,track=0,brightness=95,on=true,channel='TPS';
const trackNames=['Night Drive','Open Sky'];
function frame(content,x,y,w,h,label){
  const scale=F.geometry.display/F.geometry.master;
  return '<svg xmlns="http://www.w3.org/2000/svg" width="'+w*scale+'" height="'+h*scale+'" viewBox="'+[x,y,w,h].join(' ')+'" role="group" aria-label="'+U.esc(label)+'">'+content+'</svg>';
}
const roles={canvas:'外页背景',face:'表盘底色',panel:'控件表面',line:'细线与刻度',text:'主要读数',muted:'辅助文字',amber:'选中与强调',red:'报警',green:'正常状态',blue:'连接信息',night:'夜间读数'};
document.getElementById('palette').innerHTML=Object.entries(C).map(([key,value])=>'<article class="swatch" style="--swatch:'+value+'"><div class="swatch-color"></div><div class="swatch-copy"><p class="swatch-name">'+roles[key]+'</p><p class="swatch-role">'+key+'</p><code>'+value+'</code></div></article>').join('');
function readouts(){
  const demos=[
    {size:'primary',name:'PRIMARY',label:'OIL TEMP',value:'104',warning:'130',unit:'°C',y:220},
    {size:'compact',name:'COMPACT',label:'OIL TEMP',value:'104',warning:'130',unit:'°C',y:220},
    {size:'eccentric',name:'ECCENTRIC',label:'PRESSURE',value:'3.8',warning:'0.5',unit:'bar',y:220},
    {size:'analog',name:'ANALOG',label:'',value:'0.68',warning:'2.8',unit:'bar',y:266}
  ];
  document.getElementById('readouts').innerHTML=demos.map(d=>{
    const style=F.readout[d.size],x=360-style.width/2;
    const svg=U.readout({x,y:d.y,label:d.label,value:readoutState==='warning'?d.warning:d.value,unit:d.unit,size:d.size,state:readoutState});
    const top=d.y-(d.size==='analog'?24:12),height=d.size==='analog'?178:d.size==='compact'?184:304;
    return '<article class="component-card"><p class="component-name">'+d.name+' / '+style.valueSize/2+'px</p><div class="sample">'+frame(svg,x-12,top,style.width+24,height,d.name+' 读数')+'</div><p class="component-detail">Size='+d.size+' · State='+readoutState+'<br>Label / Value / Unit 共用中心轴</p></article>';
  }).join('');
  app.querySelectorAll('[data-state]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.state===readoutState)));
}
const actions=[{variant:'secondary',label:'CONNECT',act:'connect'},{variant:'primary',label:'CONFIRM',act:'confirm'},{variant:'ghost',label:'BACK',act:'back'},{variant:'danger',label:'CLEAR',act:'clear'},{variant:'secondary',label:'CONNECT',act:'disabled',disabled:true}];
document.getElementById('actions').innerHTML=actions.map(a=>'<article class="component-card"><p class="component-name">'+(a.disabled?'DISABLED':a.variant.toUpperCase())+'</p><div class="sample">'+frame(U.actionButton({x:240,y:260,width:240,...a}),230,250,260,108,a.disabled?'不可用操作键':a.variant+' 操作键')+'</div><p class="component-detail">'+(a.variant==='ghost'?'44px 点击区域 · 无框返回':'44px 高度 · 深色底 · 细描边')+'</p></article>').join('');
function controls(){
  const media=U.mediaButton(200,540,96,'上一首','prev-track','previous')+U.mediaButton(312,540,96,playing?'暂停':'播放','play',playing?'pause':'play')+U.mediaButton(424,540,96,'下一首','next-track','next');
  document.getElementById('media-demo').innerHTML=frame(media,194,526,332,116,'音乐控制组件');
  document.getElementById('media-note').textContent=trackNames[track]+' · '+(playing?'播放中':'已暂停');
  const adjust=U.roundButton(246,300,96,'−','minus',brightness<=10)+U.roundButton(378,300,96,'+','plus',brightness>=100);
  document.getElementById('adjust-demo').innerHTML=frame(adjust,240,294,240,100,'亮度加减组件');
  document.getElementById('brightness-value').textContent=brightness+'% · 到达边界时加减键不可用';
  document.getElementById('toggle-demo').innerHTML=frame(U.toggleControl(474,286,104,'蓝牙可发现','visible',on),468,280,116,104,'开关组件');
  document.getElementById('connection-demo').innerHTML=frame(U.connectionRow({y:180,label:'OBD ADAPTER',detail:'OBDLink CX',status:'SAVED ADAPTER',act:'connection-example',selected:true}),130,168,460,124,'连接列表行组件');
  document.getElementById('channel-demo').innerHTML=frame(U.readoutControl(channel,channel==='TPS'?'38%':'21%'),369,312,242,152,'TPS / B.P 整体切换组件');
}
const g=F.geometry;
document.getElementById('geometry-demo').innerHTML='<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720" role="group" aria-label="圆屏内容和交互安全区">'+U.shell()+'<circle cx="360" cy="360" r="'+g.interactiveRadius+'" fill="none" stroke="'+C.amber+'" stroke-width="2" stroke-dasharray="8 8"/>'+U.path('M360 200V520M200 360H520','line',2)+U.t(360,250,'CONTENT / R342',F.typography.sizes.title,'muted')+U.t(360,308,'TOUCH / R318',F.typography.sizes.title,'amber')+U.t(360,422,'720 → 360',F.typography.sizes.label,'text')+'</svg>';
function act(action){
  if(action==='connection-example')document.getElementById('connection-note').textContent='已选择 OBDLink CX · 整行点击与 Enter / Space 使用同一动作';
  else if(action==='play')playing=!playing;
  else if(action==='prev-track')track=(track+trackNames.length-1)%trackNames.length;
  else if(action==='next-track')track=(track+1)%trackNames.length;
  else if(action==='plus')brightness=Math.min(100,brightness+5);
  else if(action==='minus')brightness=Math.max(10,brightness-5);
  else if(action==='visible')on=!on;
  else if(action==='channel')channel=channel==='TPS'?'B.P':'TPS';
  else document.getElementById('action-note').textContent=({connect:'已触发连接示例',confirm:'已触发确认示例',back:'已触发返回示例',clear:'已触发清除示例'})[action]||'示例操作';
  controls();
}
app.addEventListener('click',event=>{
  const state=event.target.closest('[data-state]');
  if(state){readoutState=state.dataset.state;readouts();return;}
  const control=event.target.closest('[data-act]');
  if(control&&control.getAttribute('aria-disabled')!=='true')act(control.dataset.act);
});
app.addEventListener('keydown',event=>{
  if((event.key==='Enter'||event.key===' ')&&event.target.matches('.dial-control')&&event.target.getAttribute('aria-disabled')!=='true'){
    event.preventDefault();act(event.target.dataset.act);
  }
});
readouts();controls();
const alertDemos=[
 {name:'ADVISORY / 提醒',label:'RPM',value:'7,200',unit:'rpm',field:'rpm',reason:'SHIFT / 7,200',severity:'advisory'},
 {name:'WARNING / 异常',label:'OIL TEMP',value:'130',unit:'°C',field:'oiltemp',reason:'OIL TEMP HIGH',severity:'warning'},
 {name:'CRITICAL / 危急',label:'OIL PRESS',value:'0.5',unit:'bar',field:'oilpress',reason:'OIL PRESS LOW',severity:'critical'}
];
document.getElementById('alert-demos').innerHTML=alertDemos.map(d=>{
 const a={field:d.field,reason:d.reason,severity:d.severity};
 const svg=U.shell()+U.header('ALERT / '+d.severity.toUpperCase())+U.alertField(d.field,U.readout({x:200,y:220,label:d.label,value:d.value,unit:d.unit,state:d.severity==='advisory'?'normal':'warning',color:'amber'}),a)+U.alertMessage([a])+U.alertRim([a]);
 return '<article class="component-card"><p class="component-name">'+d.name+'</p><div class="alert-preview">'+frame(svg,0,0,720,720,d.name)+'</div><p class="component-detail">'+d.reason+'</p></article>';
}).join('');
})(window);
