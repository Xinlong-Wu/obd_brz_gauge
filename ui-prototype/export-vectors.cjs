const fs=require('fs'),path=require('path'),vm=require('vm');
const dir=__dirname;
fs.mkdirSync(path.join(dir,'vectors'),{recursive:true});
for(const file of ['foundations.js','art.js','components.js','scenarios.js','nc-gauges.js'])
  vm.runInThisContext(fs.readFileSync(path.join(dir,file),'utf8'));
const F=NC_FOUNDATIONS;
const font=fs.readFileSync(path.join(dir,F.fonts.find(f=>f.family===F.typography.number&&f.weight===500).file)).toString('base64');
const mono=fs.readFileSync(path.join(dir,F.fonts.find(f=>f.family===F.typography.label&&f.weight===500).file)).toString('base64');
const styles='<style>@font-face{font-family:'+F.typography.number+';src:url(data:font/ttf;base64,'+font+')}@font-face{font-family:'+F.typography.label+';src:url(data:font/ttf;base64,'+mono+')} .night-colors text[fill="'+F.colors.text+'"]{fill:'+F.colors.night+'}</style>';
const scenarioBoards=NC.pages.flatMap(p=>NC.scenarios[p.id].filter(scene=>scene.id!=='normal').flatMap(scene=>{
 const wheels=p.id==='tyres'&&scene.id!=='multiple'?NC_SCENARIOS.wheels:[null];
 return wheels.map(wheel=>({name:p.id+'-'+scene.id+(wheel?'-'+wheel.toLowerCase():''),id:p.id,state:{...NC.defaults,scenario:scene.id,...(wheel?{tyreWheel:wheel}:{})}}));
}));
const unavailableBoards=NC.pages.filter(p=>NC.resolve(p.id,NC.defaults).vehicle).map(p=>({name:p.id+'-no-data',id:p.id,state:{...NC.defaults,signal:false}}));
const named=[...NC.pages.map(p=>({name:p.id,id:p.id,state:NC.defaults})),...scenarioBoards,...unavailableBoards,
{name:'oil-temp-alert',id:'oiltemp',state:{...NC.defaults,alarm:true}},
{name:'tyre-pressure-alert',id:'tyres',state:{...NC.defaults,alarm:true}},
{name:'clear-devices-confirmation',id:'bluetooth',state:{...NC.defaults,dialog:'obd'}},
{name:'obd-forget-confirmation',id:'obd',state:{...NC.defaults,scenario:'connected',dialog:'obd'}},
{name:'phone-forget-confirmation',id:'pair',state:{...NC.defaults,scenario:'phone-connected',paired:true,dialog:'phone'}},
{name:'pair-not-discoverable',id:'pair',state:{...NC.defaults,visible:false}},
{name:'music-paused',id:'music',state:{...NC.defaults,playing:false}},
{name:'multi-bp',id:'multi',state:{...NC.defaults,channel:'B.P'}},
{name:'gear-night',id:'gear',state:{...NC.defaults,night:true}},
{name:'no-signal',id:'gear',state:{...NC.defaults,signal:false}},
{name:'bluetooth-disconnected',id:'bluetooth',state:{...NC.defaults,obdBound:false,paired:false,keyboard:false}}];
for(const p of named){const s=NC.render(p.id,p.state).replace('<defs>',styles+'<defs>');fs.writeFileSync(path.join(dir,'vectors',p.name+'.svg'),s);}
const boardHeight=500+Math.ceil(NC.pages.length/4)*700;
let cells='',label='font-family="IBM Plex Mono" font-size="22" fill="#A6B0AD"';
NC.pages.forEach((p,i)=>{const x=90+(i%4)*580,y=420+Math.floor(i/4)*700;const body=NC.render(p.id,NC.defaults).replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'').replaceAll('id="round"','id="round-'+i+'"').replaceAll('url(#round)','url(#round-'+i+')');cells+='<g transform="translate('+x+' '+y+')"><text x="0" y="-30" '+label+'>'+String(i+1).padStart(2,'0')+' / '+p.english.replaceAll('&','&amp;')+'</text><g transform="scale(.7)">'+body+'</g></g>';});
const board='<svg xmlns="http://www.w3.org/2000/svg" width="2400" height="'+boardHeight+'" viewBox="0 0 2400 '+boardHeight+'">'+styles+'<rect width="2400" height="'+boardHeight+'" fill="#141718"/><text x="90" y="110" font-family="IBM Plex Mono" font-size="27" fill="#FFB35B">ROADSTER INSTRUMENTS / NC-01</text><text x="90" y="254" font-family="Barlow Condensed" font-size="148" font-weight="500" fill="#F3F0E7">MX-5 / CIRCUIT</text><text x="95" y="320" font-family="IBM Plex Mono" font-size="25" fill="#A6B0AD">JDM INSTRUMENT PROTOTYPE / COMPLETE INTERFACE COLLECTION</text>'+cells+'<text x="90" y="'+(boardHeight-50)+'" font-family="IBM Plex Mono" font-size="22" fill="#A6B0AD">NC-01 / JINBA ITTAI / 720 MASTER / 360 DISPLAY / CONCEPT DATA</text></svg>';
fs.writeFileSync(path.join(dir,'overview.svg'),board);
let alertCells='';
NC.pages.forEach((p,i)=>{
 const scenario=['triple','multi','tyres'].includes(p.id)?'multiple':NC.scenarios[p.id][1]?.id||'normal';
 const x=90+(i%4)*580,y=420+Math.floor(i/4)*700;
 const body=NC.render(p.id,{...NC.defaults,scenario}).replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'').replaceAll('id="round"','id="alert-round-'+i+'"').replaceAll('url(#round)','url(#alert-round-'+i+')');
 alertCells+='<g transform="translate('+x+' '+y+')"><text x="0" y="-30" '+label+'>'+String(i+1).padStart(2,'0')+' / '+p.english.replaceAll('&','&amp;')+'</text><g transform="scale(.7)">'+body+'</g></g>';
});
fs.writeFileSync(path.join(dir,'overview-alerts.svg'),board.replace(cells,alertCells).replace('JDM INSTRUMENT PROTOTYPE / COMPLETE INTERFACE COLLECTION','WARNING / ADVISORY / SYSTEM STATE COLLECTION'));
fs.writeFileSync(path.join(dir,'design-manifest.json'),JSON.stringify({title:'MX-5 NC / Circuit Instruments',resolution:{master:720,display:360},pages:NC.pages,states:named.map(p=>p.name),scenarios:NC.scenarios,limits:NC_SCENARIOS.limits,artboards:named.map(p=>({name:p.name,page:p.id,scenario:NC_SCENARIOS.selected(p.id,p.state).id,connected:NC.resolve(p.id,p.state).connected,tyreWheel:p.state.tyreWheel})),colors:NC.colors,fonts:['Barlow Condensed','IBM Plex Mono'],figma:'https://www.figma.com/design/4Ah7vldYa2qIrVheZ0Xzc8'},null,2));
console.log('Exported '+named.length+' SVG artboards and overview.');
