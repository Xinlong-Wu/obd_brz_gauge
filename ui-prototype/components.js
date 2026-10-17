(function(root){
'use strict';
const F=root.NC_FOUNDATIONS,C=F.colors;
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function r(x,y,w,h,color,rad=0){return '<rect x="'+x+'" y="'+y+'" width="'+w+'" height="'+h+'" rx="'+rad+'" fill="'+C[color]+'"/>';}
function path(d,color='line',width=F.stroke.hairline,extra=''){return '<path d="'+d+'" fill="none" stroke="'+C[color]+'" stroke-width="'+width+'" '+extra+'/>';}
function pt(cx,cy,rad,a){return [(cx+rad*Math.cos(a*Math.PI/180)).toFixed(2),(cy+rad*Math.sin(a*Math.PI/180)).toFixed(2)];}
function arc(cx,cy,rad,a,b){return 'M'+pt(cx,cy,rad,a).join(' ')+' A'+rad+' '+rad+' 0 '+(b-a>180?1:0)+' 1 '+pt(cx,cy,rad,b).join(' ');}
function typeText({x,y,text,role,color='text',anchor='middle',baseline='hanging'}){
 const spec=Object.hasOwn(F.typography.roles,role)?F.typography.roles[role]:null;
 if(!spec)throw new RangeError('Unknown typography role: '+role);
 if(![x,y].every(Number.isFinite))throw new RangeError('Text coordinates must be finite.');
 return '<text data-type-role="'+role+'" x="'+x+'" y="'+y+'" font-family="'+spec.family+'" font-size="'+spec.size+'" font-weight="'+spec.weight+'" letter-spacing="'+spec.letterSpacing+'" fill="'+(C[color]||color)+'" text-anchor="'+anchor+'" dominant-baseline="'+baseline+'">'+esc(text)+'</text>';
}
function textLine(x,y,text,role='label',color='text',anchor='middle'){return typeText({x,y,text,role,color,anchor});}
function art(k){
 const source=(root.NC_ART[k]||'').replace(/__NC_COLOR_(\w+)__/g,(_,role)=>C[role]);
 const semantic=source.replace(/<text\b([^>]*)>([^<]*)<\/text>/g,(_,attrs,text)=>{
  const prop=name=>attrs.match(new RegExp('(?:^|\\s)'+name+'="([^\"]*)"'))?.[1];
  return typeText({x:Number(prop('x')),y:Number(prop('y')),text,role:prop('data-type-role'),color:prop('fill'),anchor:prop('text-anchor')||'start',baseline:'alphabetic'});
 });
 return '<g fill="none">'+semantic.replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'')+'</g>';
}
function metric(x,y,label,value,unit,compact=false,color='text',state='normal',labelRole='label'){
return readout({x,y,label,value,unit,size:compact?'compact':'primary',color,state,labelRole});}
function readout({x,y,label='',labelRole='label',value,unit='',size='primary',state='normal',color}){
const style=Object.hasOwn(F.readout,size)?F.readout[size]:null;
if(!style)throw new RangeError('Unknown Readout size: '+size);
if(!F.states.includes(state))throw new RangeError('Unknown Readout state: '+state);
if(![x,y].every(Number.isFinite))throw new RangeError('Readout coordinates must be finite master values.');
if(state!=='disconnected'&&(value===undefined||value===null||value===''))throw new RangeError('A connected Readout needs a value.');
const cx=x+style.width/2,ink=state==='warning'?'red':color||'text',display=state==='disconnected'?'—':value;
return (label?textLine(cx,y,label,labelRole,'muted'):'')+textLine(cx,y+style.valueOffset,display,style.valueRole,ink)+(unit?textLine(cx,y+style.unitOffset,unit,'unit','muted'):'');
}
function alertInk(alert){return alert?.severity==='advisory'?'amber':'red';}
function alertField(field,content,alert){
 const ink=alertInk(alert);
 return '<g data-field="'+esc(field)+'" data-state="'+(alert?'alert':'normal')+'"'+(alert?' data-severity="'+alert.severity+'" aria-label="'+esc(alert.reason)+'"':'')+'>'+content+'</g>';
}
function shortReason(reason){return ({'OIL PRESS LOW':'OIL P LOW','OIL TEMP HIGH':'OIL T HIGH','COOLANT HIGH':'WATER HIGH','VOLTAGE LOW':'VOLT LOW','VOLTAGE HIGH':'VOLT HIGH','ABOVE SET RANGE':'PULSE HIGH','BELOW SET RANGE':'PULSE LOW','SHIFT / 7,200':'SHIFT'})[reason]||reason.replace('PRESS ','P ').replace('TEMP ','T ');}
function alertMessage(alerts,y=F.alerts.footerY){
 if(!alerts.length)return '';
 const first=alerts[0],label=shortReason(first.reason)+(alerts.length>1?' +'+(alerts.length-1):'');
 return '<g class="dial-alert-message" data-alert-count="'+alerts.length+'" aria-label="'+esc(alerts.map(a=>a.reason).join('; '))+'">'+textLine(360,y,label,'status',alertInk(first))+'</g>';
}
function alertRim(alerts){
 if(!alerts.some(a=>a.severity==='critical'))return '';
 return '<g class="dial-alert-rim" data-alert-rim="critical">'+path(arc(360,360,F.alerts.rimRadius,145,215),'red',F.alerts.rimWidth,'stroke-linecap="round"')+path(arc(360,360,F.alerts.rimRadius,-35,35),'red',F.alerts.rimWidth,'stroke-linecap="round"')+'</g>';
}
function needle(cx,cy,radius,angle,color='amber'){
 return '<g data-live-indicator="needle">'+path('M'+cx+' '+cy+'L'+pt(cx,cy,radius,angle).join(' '),color,F.stroke.needle)+'<circle cx="'+cx+'" cy="'+cy+'" r="9" fill="'+C[color]+'"/></g>';
}
function liveArc(cx,cy,radius,start,end,color='amber',width=6){
 return path(arc(cx,cy,radius,start,end),color,width,'data-live-indicator="arc" stroke-linecap="round"');
}
function tachometer(rpm,connected=true,color='text'){
 let output=art('tach');const count=56,lit=connected?Math.ceil(Math.min(8000,rpm)/8000*count):0;
 for(let i=0;i<count;i++){
  const start=140+i*260/count,ink=i<lit?(i>=45?'red':i>=40?'amber':color):'line';
  output+=path(arc(360,360,282,start,start+3.4),ink,i%6===0?18:10,i<lit?'data-live-indicator="rpm"':'');
 }
 return output;
}
function roadMotion(speed,connected=true){
 const velocity=connected&&Number.isFinite(speed)?Math.min(F.road.maximumPreviewSpeed,Math.max(0,speed)):0;
 const metersPerSecond=velocity/3.6;
 return {speed:velocity,moving:velocity>0,metersPerSecond,frequency:metersPerSecond/F.road.spacingMeters};
}
function roadProjection(side,phase){
 const p=((phase%1)+1)%1,depth=p*p,g=F.road;
 const left=g.farX+(g.nearX-g.farX)*depth,x=side<0?left:F.geometry.master-left;
 const y=g.farY+(g.nearY-g.farY)*depth,scale=g.farScale+(g.nearScale-g.farScale)*depth;
 const opacity=Math.min(1,p/.12,(1-p)/.12)*(.4+.6*depth);
 return {x,y,scale,opacity,transform:'translate('+x.toFixed(2)+' '+y.toFixed(2)+') scale('+scale.toFixed(4)+')'};
}
function roadScene(speed,connected=true,distance=0){
 const motion=roadMotion(speed,connected),g=F.road,travel=Number.isFinite(distance)?distance:0;
 let content='<g class="roadside-scene" data-road-speed="'+motion.speed+'" data-road-frequency="'+motion.frequency.toFixed(3)+'" data-road-state="'+(motion.moving?'moving':'stopped')+'" aria-hidden="true" pointer-events="none">';
 content+='<defs><clipPath id="roadside-clip"><circle cx="360" cy="360" r="'+g.clipRadius+'"/></clipPath></defs><g clip-path="url(#roadside-clip)">';
 for(const side of [-1,1])for(let slot=0;slot<g.objectsPerSide;slot++){
  const phase=slot/g.objectsPerSide+(side>0?.11:0)+travel/(g.spacingMeters*g.objectsPerSide),p=roadProjection(side,phase);
  const building=(slot+(side>0?1:0))%3===1;
  const silhouette=building?'M-20 0V-70H-7V-84H20V0Z':'M0 0V-12M-18-12H18L8-30H13L0-59L-13-30H-8Z';
  const detail=building?'M-11-58H-5M5-58H11M-11-43H-5M5-43H11M-11-28H-5M5-28H11M-4 0V-12H5V0':'M-7 0H7';
  const fill=building?C.panel:g.colors.treeFill,outline=building?C.muted:g.colors.treeOutline;
  const windows=building?'<path data-road-lit-windows="subtle" d="M-11-58H-5M5-43H11" fill="none" stroke="'+g.colors.window+'" stroke-width="3" stroke-linecap="square"/>':'';
  content+='<g class="roadside-object" data-road-side="'+side+'" data-road-slot="'+slot+'" data-road-kind="'+(building?'building':'tree')+'" transform="'+p.transform+'" opacity="'+p.opacity.toFixed(3)+'"><path d="'+silhouette+'" fill="'+fill+'" stroke="'+outline+'" stroke-width="'+g.strokeWidth+'" stroke-linejoin="round"/><path d="'+detail+'" fill="none" stroke="'+C.line+'" stroke-width="2" stroke-linecap="round"/>'+windows+'</g>';
 }
 return content+'</g></g>';
}
function statusText(x,y,label,state='normal'){
return textLine(x,y,label,'status',state==='warning'?'red':state==='disconnected'?'muted':'green');
}
function header(title,sub=''){return textLine(F.geometry.center,92,title,'pageTitle','amber')+(sub?textLine(F.geometry.center,142,sub,'meta','muted'):'');}
function shell(status=''){return '<circle cx="'+F.geometry.center+'" cy="'+F.geometry.center+'" r="'+F.geometry.radius+'" fill="'+C.face+'"/><circle cx="360" cy="360" r="'+F.geometry.rimRadius+'" fill="none" stroke="'+C.line+'" stroke-width="2"/>'+r(356,32,8,24,'amber',3)+(status?textLine(360,648,status,'status',status==='NO SIGNAL'?'amber':'muted'):'');}
function chip(x,y,w,label,color='panel'){return r(x,y,w,52,color,8)+textLine(x+w/2,y+9,label,'status',color==='amber'||color==='red'?'face':'text');}
function btn(x,y,w,label,act,color='panel',options={}){
const back=options.back??(act==='back'||act==='connections'),ghost=options.variant?options.variant==='ghost':back||act==='forget',danger=options.variant?options.variant==='danger':color==='red',primary=options.variant?options.variant==='primary':color==='amber',disabled=options.disabled===true;
const h=F.controls.actionHeight,cut=F.spacing.xs;
const lines=label.split('\n'),font=F.typography.roles.action.size,lineHeight=font+6,start=y+(h-((lines.length-1)*lineHeight+font))/2-2,ink=danger?'red':'text';
const d='M'+(x+cut)+' '+y+'H'+(x+w-cut)+'L'+(x+w)+' '+(y+cut)+'V'+(y+h-cut)+'L'+(x+w-cut)+' '+(y+h)+'H'+(x+cut)+'L'+x+' '+(y+h-cut)+'V'+(y+cut)+'Z';
const surface=ghost?path('M'+(x+12)+' '+(y+76)+'H'+(x+w-12),'amber',2,'class="control-focus" opacity="0"'):'<path class="control-surface" d="'+d+'" fill="'+C.panel+'" stroke="'+C[danger?'red':primary?'amber':'line']+'" stroke-width="2"/>';
const arrow=back?path('M'+(x+w/2-50)+' '+(y+32)+'L'+(x+w/2-62)+' '+(y+44)+'L'+(x+w/2-50)+' '+(y+56)+'M'+(x+w/2-62)+' '+(y+44)+'H'+(x+w/2-44),'text',3,'class="control-symbol" stroke-linecap="round" stroke-linejoin="round"'):'';
const words=lines.map((line,i)=>textLine(x+w/2+(back?12:0),start+i*lineHeight,line,'action',ink).replace('<text ','<text class="control-label" ')).join('');
return '<g class="dial-control precision-key action-key '+(danger?'precision-danger':primary?'precision-primary':ghost?'precision-ghost':'precision-secondary')+'" role="button" tabindex="'+(disabled?-1:0)+'" aria-label="'+esc(lines.join(' '))+'" data-act="'+act+'"'+(disabled?' aria-disabled="true" opacity="'+F.controls.disabledOpacity+'"':'')+'>'+controlHit(x,y,w,F.controls.actionHeight)+surface+arrow+words+'</g>';}
function actionButton({x,y,width,label,act='example',variant='secondary',disabled=false,back}){
if(!F.actionVariants.includes(variant))throw new RangeError('Unknown Action variant: '+variant);
return btn(x,y,width,label,act,'panel',{variant,disabled,back});
}
function validateHitRegion({x,y,width,height}){
if(![x,y,width,height].every(Number.isFinite))throw new RangeError('Hit region must use finite master coordinates.');
if(width<F.controls.minimumHit||height<F.controls.minimumHit)throw new RangeError('Hit region must be at least 44×44 at 360 display size.');
const c=F.geometry.center;
if([[x,y],[x+width,y],[x,y+height],[x+width,y+height]].some(([a,b])=>Math.hypot(a-c,b-c)>F.geometry.interactiveRadius))throw new RangeError('Hit region exceeds the circular interactive safe area.');
return true;
}
function controlHit(x,y,w,h){validateHitRegion({x,y,width:w,height:h});return '<rect class="control-hit" x="'+x+'" y="'+y+'" width="'+w+'" height="'+h+'" fill="transparent" stroke="none"/>';}
function roundButton(x,y,w,label,act,disabled=false){
const cx=x+w/2,cy=y+44,d='M'+(cx-14)+' '+cy+'H'+(cx+14)+(label==='+'?'M'+cx+' '+(cy-14)+'V'+(cy+14):'');
return '<g class="dial-control precision-key round-key" role="button" tabindex="'+(disabled?-1:0)+'" aria-label="'+esc(label)+'" aria-disabled="'+disabled+'" data-act="'+act+'"'+(disabled?' opacity="'+F.controls.disabledOpacity+'"':'')+'>'+controlHit(cx-44,cy-44,88,88)+'<circle class="control-surface" cx="'+cx+'" cy="'+cy+'" r="'+F.controls.roundRadius+'" fill="'+C.panel+'" stroke="'+C.line+'" stroke-width="2"/>'+path(d,'text',3,'class="control-symbol" stroke-linecap="round"')+'</g>';
}
function toggleControl(x,y,w,label,act,on){
const thumbX=on?x+w-40:x+8;
return '<g class="dial-control precision-key toggle-key" role="switch" tabindex="0" aria-label="'+esc(label)+'" aria-checked="'+on+'" data-act="'+act+'">'+controlHit(x,y,w,88)+'<rect class="control-surface" x="'+x+'" y="'+(y+10)+'" width="'+w+'" height="38" rx="4" fill="'+C.panel+'" stroke="'+C.line+'" stroke-width="2"/>'+path('M'+(x+w/2)+' '+(y+17)+'V'+(y+41),'line',2)+'<rect class="control-thumb" x="'+thumbX+'" y="'+(y+15)+'" width="32" height="28" rx="2" fill="'+C[on?'amber':'muted']+'"/>'+textLine(x+w/2,y+54,on?'ON':'OFF','status',on?'amber':'muted').replace('<text ','<text class="control-label" ')+'</g>';
}
function readoutControl(label,value){
const selectedX=label==='TPS'?440:540;
const labels=textLine(440,324,'TPS','action',label==='TPS'?'amber':'muted')+textLine(490,324,'/','action','muted')+textLine(540,324,'B.P','action',label==='B.P'?'amber':'muted');
return '<g class="dial-control precision-key readout-key" role="button" tabindex="0" aria-label="切换 TPS / B.P，当前 '+esc(label)+'" data-act="channel">'+controlHit(375,318,230,140)+labels+path('M'+(selectedX-22)+' 358H'+(selectedX+22),'amber',3,'class="channel-selection" stroke-linecap="round"')+textLine(482,374,String(value).replace(/%$/,''),'valueCompact')+textLine(544,414,'%','unit','muted','start')+path('M410 451H570','amber',2,'class="control-focus" opacity="0"')+'</g>';
}
function connectionRow({x=142,y,width=436,label,labelRole='label',detail,detailRole='content',status='',act,selected=false}){
 const ink=selected?'amber':'text';
 return '<g class="dial-control precision-key connection-row" role="button" tabindex="0" aria-label="'+esc([label,detail,status].filter(Boolean).join(', '))+'" data-act="'+esc(act)+'"'+(selected?' aria-current="true"':'')+'>'+controlHit(x,y,width,116)+textLine(x+16,y+2,label,labelRole,'muted','start')+textLine(x+16,y+42,detail,detailRole,ink,'start')+(status?textLine(x+16,y+80,status,'status',selected?'green':'muted','start'):'')+path('M'+(x+width-28)+' '+(y+44)+'l10 10-10 10',ink,3,'class="control-symbol" stroke-linecap="round" stroke-linejoin="round"')+path('M'+(x+16)+' '+(y+115)+'H'+(x+width-16),selected?'amber':'line',2,'class="control-focus"')+'</g>';
}
function icon(x,y,paths,color='text',scale=1){return '<g transform="translate('+x+' '+y+') scale('+scale+')">'+path(paths,color,2.5,'stroke-linecap="round" stroke-linejoin="round"')+'</g>';}
function mediaButton(x,y,w,label,act,kind,color='panel',disabled=false){
const cx=x+w/2,cy=y+44,primary=act==='play',hit=primary?F.controls.musicHit:F.controls.minimumHit;
const d=kind==='pause'?'M'+(cx-10)+' '+(cy-18)+'v36M'+(cx+10)+' '+(cy-18)+'v36':kind==='play'?'M'+(cx-12)+' '+(cy-19)+'L'+(cx+18)+' '+cy+'L'+(cx-12)+' '+(cy+19)+'Z':kind==='previous'?'M'+(cx-18)+' '+(cy-17)+'v34M'+(cx+16)+' '+(cy-17)+'L'+(cx-10)+' '+cy+'L'+(cx+16)+' '+(cy+17)+'Z':'M'+(cx+18)+' '+(cy-17)+'v34M'+(cx-16)+' '+(cy-17)+'L'+(cx+10)+' '+cy+'L'+(cx-16)+' '+(cy+17)+'Z';
validateHitRegion({x:cx-hit/2,y:cy-hit/2,width:hit,height:hit});
const hitbox='<rect class="music-hit" x="'+(cx-hit/2)+'" y="'+(cy-hit/2)+'" width="'+hit+'" height="'+hit+'" fill="transparent" stroke="none"/>';
const surface=primary?'<circle class="music-surface" cx="'+cx+'" cy="'+cy+'" r="'+F.controls.musicRadius+'" fill="'+C.panel+'" stroke="'+C.amber+'" stroke-width="2"/>':'<circle class="music-focus" cx="'+cx+'" cy="'+cy+'" r="'+F.controls.auxiliaryFocusRadius+'" fill="none" stroke="'+C.amber+'" stroke-width="2" opacity="0" pointer-events="none"/>';
const symbol='<path class="music-symbol" d="'+d+'" fill="'+(kind==='play'?C.text:'none')+'" stroke="'+C.text+'" stroke-width="'+(primary?4:3)+'" stroke-linecap="round" stroke-linejoin="round"/>';
return '<g class="dial-control music-key '+(primary?'music-primary':'music-secondary')+'" role="button" tabindex="'+(disabled?-1:0)+'" aria-label="'+esc(label)+'"'+(disabled?' aria-disabled="true" opacity="'+F.controls.disabledOpacity+'"':'')+' data-act="'+act+'">'+hitbox+surface+symbol+'</g>';
}
function trend(vals,x,y,w,h,color='amber'){const lo=Math.min(...vals),hi=Math.max(...vals);return path(vals.map((v,i)=>(i?'L':'M')+(x+i*w/(vals.length-1)).toFixed(1)+' '+(y+h-(v-lo)/(hi-lo||1)*h).toFixed(1)).join(' '),color,3,'stroke-linecap="round" stroke-linejoin="round"');}
root.NC_COMPONENTS={esc,typeText,textLine,shortReason,r,path,pt,arc,art,metric,header,shell,chip,btn,controlHit,roundButton,toggleControl,readoutControl,connectionRow,icon,mediaButton,trend,readout,actionButton,statusText,validateHitRegion,alertInk,alertField,alertMessage,alertRim,needle,liveArc,tachometer,roadMotion,roadProjection,roadScene};
})(typeof window!=='undefined'?window:globalThis);
