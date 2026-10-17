(function(root){
'use strict';
function freeze(value){
  if(value && typeof value==='object'){
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}
const typography={number:'Barlow Condensed',label:'IBM Plex Mono',body:'sans-serif',weight:500,labelSpacing:1.2,sizes:{hero:180,eccentric:144,analog:96,boost:90,pair:82,tyre:80,compact:76,mediaTitle:60,statistic:50,body:30,title:28,label:26,support:24,small:22}};
// Semantic roles are shared by every instrument. Legacy sizes remain for firmware compatibility.
const role=(size,family=typography.label,weight=500)=>({size,family,weight,letterSpacing:family===typography.label?typography.labelSpacing:0});
typography.roles={
  valuePrimary:role(180,typography.number),valueCompact:role(80,typography.number),valueSecondary:role(50,typography.number),
  pairCode:role(80,typography.number,600),contentTitle:role(60,typography.number,600),pageTitle:role(36),
  label:role(32),action:role(32),status:role(32),content:role(32),
  unit:role(28),scale:role(28),meta:role(28)
};
root.NC_FOUNDATIONS=freeze({
  version:2,
  colors:{canvas:'#141718',face:'#090D0F',panel:'#151C1F',line:'#334046',text:'#F3F0E7',muted:'#A6B0AD',amber:'#FFB35B',red:'#FF4D4D',green:'#06D6A0',blue:'#91C6DB',night:'#E8B27A'},
  viewer:{panel:'#1a1f21',secondary:'#85918f',hover:'#1e282d',pressed:'#26343a',danger:'#29191c',typography:{title:28,body:14,label:13,meta:12}},
  geometry:{master:720,display:360,center:360,radius:360,rimRadius:342,interactiveRadius:318},
  typography,
  spacing:{xs:8,sm:16,md:24,lg:32},
  stroke:{hairline:2,needle:5},
  alerts:{rimRadius:330,rimWidth:4,footerY:638,fieldGap:8},
  road:{spacingMeters:30,objectsPerSide:5,maximumPreviewSpeed:220,farX:226,nearX:110,farY:372,nearY:575,farScale:0.14,nearScale:1,clipRadius:330,strokeWidth:2.6,colors:{treeOutline:'#779D7B',treeFill:'#2A4532',window:'#B19668'}},
  controls:{minimumHit:88,actionHeight:88,musicHit:104,musicRadius:51,roundRadius:43,auxiliaryFocusRadius:40,disabledOpacity:0.4,feedbackMs:120},
  readout:{
    primary:{width:320,valueRole:'valuePrimary',valueSize:typography.roles.valuePrimary.size,valueOffset:44,unitOffset:232},
    compact:{width:230,valueRole:'valueCompact',valueSize:typography.roles.valueCompact.size,valueOffset:40,unitOffset:130},
    eccentric:{width:320,valueRole:'valuePrimary',valueSize:typography.roles.valuePrimary.size,valueOffset:40,unitOffset:232},
    analog:{width:320,valueRole:'valuePrimary',valueSize:typography.roles.valuePrimary.size,valueOffset:0,unitOffset:182}
  },
  states:['normal','warning','disconnected'],
  actionVariants:['secondary','primary','ghost','danger'],
  fonts:[
    {family:typography.number,file:'fonts/barlow-medium.ttf',weight:500},
    {family:typography.number,file:'fonts/barlow-semibold.ttf',weight:600},
    {family:typography.label,file:'fonts/plex-regular.ttf',weight:400},
    {family:typography.label,file:'fonts/plex-medium.ttf',weight:500}
  ],
  figma:{fileKey:'4Ah7vldYa2qIrVheZ0Xzc8',page:'03 / Components & foundations',readout:'Size / State',action:'Style',shell:'Gauge Shell'}
});
})(typeof window!=='undefined'?window:globalThis);
