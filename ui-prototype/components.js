(function(root){
'use strict';
const F=root.NC_FOUNDATIONS,C=F.colors;
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function t(x,y,s,size=F.typography.sizes.label,color='text',anchor='middle',family=F.typography.label,weight=F.typography.weight){
return '<text x="'+x+'" y="'+y+'" font-family="'+family+'" font-size="'+size+'" font-weight="'+weight+'" fill="'+(C[color]||color)+'" text-anchor="'+anchor+'" dominant-baseline="hanging"'+(family===F.typography.label?' letter-spacing="'+F.typography.labelSpacing+'"':'')+'>'+esc(s)+'</text>';}
function r(x,y,w,h,color,rad=0){return '<rect x="'+x+'" y="'+y+'" width="'+w+'" height="'+h+'" rx="'+rad+'" fill="'+C[color]+'"/>';}
function path(d,color='line',width=F.stroke.hairline,extra=''){return '<path d="'+d+'" fill="none" stroke="'+C[color]+'" stroke-width="'+width+'" '+extra+'/>';}
function pt(cx,cy,rad,a){return [(cx+rad*Math.cos(a*Math.PI/180)).toFixed(2),(cy+rad*Math.sin(a*Math.PI/180)).toFixed(2)];}
function arc(cx,cy,rad,a,b){return 'M'+pt(cx,cy,rad,a).join(' ')+' A'+rad+' '+rad+' 0 '+(b-a>180?1:0)+' 1 '+pt(cx,cy,rad,b).join(' ');}
function art(k){return '<g fill="none">'+(root.NC_ART[k]||'').replace(/__NC_COLOR_(\w+)__/g,(_,role)=>C[role]).replace(/__NC_FONT_(\w+)__/g,(_,role)=>F.typography[role]).replace(/__NC_SIZE_(\w+)__/g,(_,role)=>F.typography.sizes[role]).replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'')+'</g>';}
function metric(x,y,label,value,unit,compact=false,color='text',state='normal'){
return readout({x,y,label,value,unit,size:compact?'compact':'primary',color,state});}
function readout({x,y,label='',value,unit='',size='primary',state='normal',color}){
const style=Object.hasOwn(F.readout,size)?F.readout[size]:null;
if(!style)throw new RangeError('Unknown Readout size: '+size);
if(!F.states.includes(state))throw new RangeError('Unknown Readout state: '+state);
if(![x,y].every(Number.isFinite))throw new RangeError('Readout coordinates must be finite master values.');
if(state!=='disconnected'&&(value===undefined||value===null||value===''))throw new RangeError('A connected Readout needs a value.');
const cx=x+style.width/2,ink=state==='warning'?'red':color||'text',display=state==='disconnected'?'—':value;
return (label?t(cx,y,label,style.labelSize,'muted'):'')+t(cx,y+style.valueOffset,display,style.valueSize,ink,'middle',F.typography.number,F.typography.weight)+(unit?t(cx,y+style.unitOffset,unit,style.unitSize,'muted'):'');
}
function alertInk(alert){return alert?.severity==='advisory'?'amber':'red';}
function alertField(field,content,alert){
 const ink=alertInk(alert);
 return '<g data-field="'+esc(field)+'" data-state="'+(alert?'alert':'normal')+'"'+(alert?' data-severity="'+alert.severity+'" aria-label="'+esc(alert.reason)+'"':'')+'>'+content+'</g>';
}
function alertMessage(alerts,y=F.alerts.footerY){
 if(!alerts.length)return '';
 const first=alerts[0],label=first.reason+(alerts.length>1?' +'+(alerts.length-1):'');
 return '<g class="dial-alert-message" data-alert-count="'+alerts.length+'" aria-label="'+esc(alerts.map(a=>a.reason).join('; '))+'">'+t(360,y,label,F.typography.sizes.small,alertInk(first))+'</g>';
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
  content+='<g class="roadside-object" data-road-side="'+side+'" data-road-slot="'+slot+'" data-road-kind="'+(building?'building':'tree')+'" transform="'+p.transform+'" opacity="'+p.opacity.toFixed(3)+'"><path d="'+silhouette+'" fill="'+C.panel+'" stroke="'+C.muted+'" stroke-width="'+g.strokeWidth+'" stroke-linejoin="round"/><path d="'+detail+'" fill="none" stroke="'+C.line+'" stroke-width="2" stroke-linecap="round"/></g>';
 }
 return content+'</g></g>';
}
function statusText(x,y,label,state='normal'){
return t(x,y,label,F.typography.sizes.support,state==='warning'?'red':state==='disconnected'?'muted':'green');
}
function header(title,sub=''){return t(F.geometry.center,92,title,F.typography.sizes.title,'amber')+(sub?t(F.geometry.center,135,sub,F.typography.sizes.small,'muted'):'');}
function shell(status=''){return '<circle cx="'+F.geometry.center+'" cy="'+F.geometry.center+'" r="'+F.geometry.radius+'" fill="'+C.face+'"/><circle cx="360" cy="360" r="'+F.geometry.rimRadius+'" fill="none" stroke="'+C.line+'" stroke-width="2"/>'+r(356,32,8,24,'amber',3)+(status?t(360,658,status,F.typography.sizes.support,status==='NO SIGNAL'?'amber':'muted'):'');}
function chip(x,y,w,label,color='panel'){return r(x,y,w,44,color,8)+t(x+w/2,y+10,label,F.typography.sizes.small,color==='amber'||color==='red'?'face':'text');}
function btn(x,y,w,label,act,color='panel',options={}){
const back=options.back??(act==='back'||act==='connections'),ghost=options.variant?options.variant==='ghost':back||act==='forget',danger=options.variant?options.variant==='danger':color==='red',primary=options.variant?options.variant==='primary':color==='amber',disabled=options.disabled===true;
const h=F.controls.actionHeight,cut=F.spacing.xs;
const lines=label.split('\n'),font=lines.length>1||back?F.typography.sizes.small:F.typography.sizes.label,lineHeight=font+6,start=y+(h-((lines.length-1)*lineHeight+font))/2-2,ink=danger?'red':'text';
const d='M'+(x+cut)+' '+y+'H'+(x+w-cut)+'L'+(x+w)+' '+(y+cut)+'V'+(y+h-cut)+'L'+(x+w-cut)+' '+(y+h)+'H'+(x+cut)+'L'+x+' '+(y+h-cut)+'V'+(y+cut)+'Z';
const surface=ghost?path('M'+(x+12)+' '+(y+76)+'H'+(x+w-12),'amber',2,'class="control-focus" opacity="0"'):'<path class="control-surface" d="'+d+'" fill="'+C.panel+'" stroke="'+C[danger?'red':primary?'amber':'line']+'" stroke-width="2"/>';
const arrow=back?path('M'+(x+w/2-30)+' '+(y+32)+'L'+(x+w/2-42)+' '+(y+44)+'L'+(x+w/2-30)+' '+(y+56)+'M'+(x+w/2-42)+' '+(y+44)+'H'+(x+w/2-24),'text',3,'class="control-symbol" stroke-linecap="round" stroke-linejoin="round"'):'';
const words=lines.map((line,i)=>t(x+w/2+(back?14:0),start+i*lineHeight,line,font,ink).replace('<text ','<text class="control-label" ')).join('');
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
return '<g class="dial-control precision-key toggle-key" role="switch" tabindex="0" aria-label="'+esc(label)+'" aria-checked="'+on+'" data-act="'+act+'">'+controlHit(x,y,w,88)+'<rect class="control-surface" x="'+x+'" y="'+(y+10)+'" width="'+w+'" height="38" rx="4" fill="'+C.panel+'" stroke="'+C.line+'" stroke-width="2"/>'+path('M'+(x+w/2)+' '+(y+17)+'V'+(y+41),'line',2)+'<rect class="control-thumb" x="'+thumbX+'" y="'+(y+15)+'" width="32" height="28" rx="2" fill="'+C[on?'amber':'muted']+'"/>'+t(x+w/2,y+56,on?'ON':'OFF',F.typography.sizes.small,on?'amber':'muted').replace('<text ','<text class="control-label" ')+'</g>';
}
function readoutControl(label,value){
const selectedX=label==='TPS'?450:530;
const labels=t(450,328,'TPS',F.typography.sizes.support,label==='TPS'?'amber':'muted')+t(490,328,'/',F.typography.sizes.support,'muted')+t(530,328,'B.P',F.typography.sizes.support,label==='B.P'?'amber':'muted');
return '<g class="dial-control precision-key readout-key" role="button" tabindex="0" aria-label="切换 TPS / B.P，当前 '+esc(label)+'" data-act="channel">'+controlHit(375,318,230,140)+labels+path('M'+(selectedX-22)+' 358H'+(selectedX+22),'amber',3,'class="channel-selection" stroke-linecap="round"')+t(490,378,value,F.typography.sizes.compact,'text','middle',F.typography.number)+path('M410 451H570','amber',2,'class="control-focus" opacity="0"')+'</g>';
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
root.NC_COMPONENTS={esc,t,r,path,pt,arc,art,metric,header,shell,chip,btn,controlHit,roundButton,toggleControl,readoutControl,icon,mediaButton,trend,readout,actionButton,statusText,validateHitRegion,alertInk,alertField,alertMessage,alertRim,needle,liveArc,tachometer,roadMotion,roadProjection,roadScene};
})(typeof window!=='undefined'?window:globalThis);
