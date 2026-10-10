const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
function load(){
 const context=vm.createContext({});
 for(const file of ['foundations.js','art.js','components.js','scenarios.js','nc-gauges.js'])vm.runInContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),context);
 return context.NC;
}
test('all 16 screens expose normal plus their supported preview scenarios',()=>{
 const NC=load();
 for(const p of NC.pages){
  assert.ok(NC.scenarios[p.id],p.id);
  assert.equal(NC.scenarios[p.id][0].id,'normal');
  assert.equal(new Set(NC.scenarios[p.id].map(s=>s.id)).size,NC.scenarios[p.id].length);
  for(const scene of NC.scenarios[p.id]){
   const svg=NC.render(p.id,{...NC.defaults,scenario:scene.id});
   assert.doesNotMatch(svg,/undefined|NaN|__NC_/);
  }
 }
 assert.equal(NC.scenarios.display.length,1);
});
test('disconnection suppresses limit warnings and active gauge graphics together',()=>{
 const NC=load();
 for(const p of NC.pages.filter(p=>NC.resolve(p.id,NC.defaults).vehicle)){
  for(const scene of NC.scenarios[p.id]){
   const state={...NC.defaults,scenario:scene.id,signal:false},svg=NC.render(p.id,state);
   assert.equal(NC.resolve(p.id,state).alerts.length,0,p.id);
   assert.doesNotMatch(svg,/data-live-indicator|data-state="alert"|data-alert-rim|CRUISE ACTIVE|SET 100|TEMP HIGH|SHIFT \/ 7,200/);
   assert.match(svg,/NO SIGNAL/);
  }
 }
 assert.doesNotMatch(NC.render('tyres',{...NC.defaults,signal:false}),/35°C|37°C/);
});
test('composite scenarios mark each abnormal item and rank critical oil pressure first',()=>{
 const NC=load();
 for(const id of ['triple','multi']){
  const state={...NC.defaults,scenario:'multiple'},m=NC.resolve(id,state),svg=NC.render(id,state);
  assert.equal(m.alerts.length,3);
  assert.equal(m.alerts[0].field,'oilpress');
  assert.equal((svg.match(/data-state="alert"/g)||[]).length,3);
  assert.match(svg,/data-alert-count="3"/);
  assert.match(svg,/data-alert-rim="critical"/);
 }
 const svg=NC.render('multi',{...NC.defaults,scenario:'voltage-high'});
 assert.match(svg,/VOLTAGE HIGH/);assert.match(svg,/>16.4<\/text>/);
 assert.doesNotMatch(svg,/data-field="oiltemp" data-state="alert"/);
});
test('pressure screens share the same alert reason and pressure sample',()=>{
 const NC=load(),state={...NC.defaults,scenario:'pressure-high'};
 for(const id of ['boost','analog']){
  const m=NC.resolve(id,state),svg=NC.render(id,state);
  assert.equal(m.values.boost,2.6);assert.equal(m.alerts[0].reason,'BOOST HIGH');
  assert.match(svg,/>2.60<\/text>/);assert.match(svg,/data-live-indicator="needle"/);
 }
});
test('all four wheel positions support independent pressure and temperature warnings',()=>{
 const NC=load();
 for(const wheel of ['FL','FR','RL','RR'])for(const scenario of ['tyre-low','tyre-high','tyre-hot']){
  const state={...NC.defaults,scenario,tyreWheel:wheel},m=NC.resolve('tyres',state);
  assert.equal(m.alerts.length,1);assert.ok(m.alerts[0].field.startsWith(wheel+'.'));
  const svg=NC.render('tyres',state);assert.match(svg,new RegExp('data-wheel-alert="'+wheel+'"'));
  assert.equal((svg.match(/data-wheel-alert=/g)||[]).length,1);
 }
 assert.equal(NC.resolve('tyres',{...NC.defaults,scenario:'multiple'}).alerts.length,3);
});
test('optional limits and heart availability have independent state and advisory severity',()=>{
 const NC=load(),limits={speed:130,gforce:1.4,heartLow:45,heartHigh:170};
 for(const [id,scenario,value] of [['speed','speed-limit',145],['gforce','g-limit',1.6],['heart','heart-high',185],['heart','heart-low',35]]){
  const m=NC.resolve(id,{...NC.defaults,scenario,limits});
  const field=id==='heart'?'heart':id==='speed'?'speed':'lat';
  assert.ok(Math.abs(m.values[field]-value)<1e-9);assert.equal(m.alerts[0].severity,'advisory');
 }
 assert.equal(NC.resolve('heart',{...NC.defaults,signal:false}).connected,true);
 for(const state of [{...NC.defaults,scenario:'no-data'},{...NC.defaults,scenario:'heart-high',heartSignal:false}]){
  const svg=NC.render('heart',state);assert.match(svg,/NO SENSOR/);assert.doesNotMatch(svg,/data-live-indicator|ABOVE SET RANGE|>105<\/text>/);
 }
 assert.equal(NC.resolve('speed',NC.defaults).alerts.length,0);
 assert.equal(NC.resolve('gforce',NC.defaults).alerts.length,0);
});
test('system failures preserve recoverable layouts and disable unavailable media controls',()=>{
 const NC=load(),svg=NC.render('music',{...NC.defaults,scenario:'media-unavailable'});
 assert.equal((svg.match(/aria-disabled="true"/g)||[]).length,3);
 assert.equal((svg.match(/tabindex="-1"/g)||[]).length,3);
 assert.match(svg,/SOURCE UNAVAILABLE/);assert.doesNotMatch(svg,/Night Drive/);
 const lost=NC.render('bluetooth',{...NC.defaults,scenario:'connection-lost'});
 assert.match(lost,/CONNECTION LOST/);assert.match(lost,/SAVED PAIRING RETAINED/);assert.doesNotMatch(lost,/DEVICE CONNECTED/);
 for(const scenario of ['pair-failed','pair-expired'])assert.match(NC.render('pair',{...NC.defaults,scenario}),/data-act="retry-pair"/);
});
