const fs=require('fs'),path=require('path'),vm=require('vm');
const dir=__dirname;
fs.mkdirSync(path.join(dir,'vectors'),{recursive:true});
vm.runInThisContext(fs.readFileSync(path.join(dir,'art.js'),'utf8'));
vm.runInThisContext(fs.readFileSync(path.join(dir,'nc-gauges.js'),'utf8'));
const font=fs.readFileSync(path.join(dir,'fonts','barlow-medium.ttf')).toString('base64');
const mono=fs.readFileSync(path.join(dir,'fonts','plex-medium.ttf')).toString('base64');
const styles='<style>@font-face{font-family:Barlow Condensed;src:url(data:font/ttf;base64,'+font+')}@font-face{font-family:IBM Plex Mono;src:url(data:font/ttf;base64,'+mono+')} .night-colors text[fill="#F3F0E7"]{fill:#E8B27A}</style>';
const named=[...NC.pages.map(p=>({name:p.id,id:p.id,state:NC.defaults})),
{name:'oil-temp-alert',id:'oiltemp',state:{...NC.defaults,alarm:true}},
{name:'tyre-pressure-alert',id:'tyres',state:{...NC.defaults,alarm:true}},
{name:'clear-devices-confirmation',id:'bluetooth',state:{...NC.defaults,dialog:true}},
{name:'music-paused',id:'music',state:{...NC.defaults,playing:false}},
{name:'multi-bp',id:'multi',state:{...NC.defaults,channel:'B.P'}},
{name:'gear-night',id:'gear',state:{...NC.defaults,night:true}},
{name:'no-signal',id:'gear',state:{...NC.defaults,signal:false}},
{name:'bluetooth-disconnected',id:'bluetooth',state:{...NC.defaults,paired:false,keyboard:false}}];
for(const p of named){const s=NC.render(p.id,p.state).replace('<defs>',styles+'<defs>');fs.writeFileSync(path.join(dir,'vectors',p.name+'.svg'),s);}
let cells='',label='font-family="IBM Plex Mono" font-size="22" fill="#A6B0AD"';
NC.pages.forEach((p,i)=>{const x=90+(i%4)*580,y=420+Math.floor(i/4)*700;const body=NC.render(p.id,NC.defaults).replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'').replaceAll('id="round"','id="round-'+i+'"').replaceAll('url(#round)','url(#round-'+i+')');cells+='<g transform="translate('+x+' '+y+')"><text x="0" y="-30" '+label+'>'+String(i+1).padStart(2,'0')+' / '+p.english.replaceAll('&','&amp;')+'</text><g transform="scale(.7)">'+body+'</g></g>';});
const board='<svg xmlns="http://www.w3.org/2000/svg" width="2400" height="3300" viewBox="0 0 2400 3300">'+styles+'<rect width="2400" height="3300" fill="#141718"/><text x="90" y="110" font-family="IBM Plex Mono" font-size="27" fill="#FFB35B">ROADSTER INSTRUMENTS / NC-01</text><text x="90" y="254" font-family="Barlow Condensed" font-size="148" font-weight="500" fill="#F3F0E7">MX-5 / CIRCUIT</text><text x="95" y="320" font-family="IBM Plex Mono" font-size="25" fill="#A6B0AD">JDM INSTRUMENT PROTOTYPE / COMPLETE INTERFACE COLLECTION</text>'+cells+'<text x="90" y="3250" font-family="IBM Plex Mono" font-size="22" fill="#A6B0AD">NC-01 / JINBA ITTAI / 720 MASTER / 360 DISPLAY / CONCEPT DATA</text></svg>';
fs.writeFileSync(path.join(dir,'overview.svg'),board);
fs.writeFileSync(path.join(dir,'design-manifest.json'),JSON.stringify({title:'MX-5 NC / Circuit Instruments',resolution:{master:720,display:360},pages:NC.pages,states:named.map(p=>p.name),colors:NC.colors,fonts:['Barlow Condensed','IBM Plex Mono'],figma:'https://www.figma.com/design/4Ah7vldYa2qIrVheZ0Xzc8'},null,2));
console.log('Exported '+named.length+' SVG artboards and overview.');
