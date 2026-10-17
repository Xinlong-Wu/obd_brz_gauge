const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
function load(){
 const c=vm.createContext({});
 for(const f of ['foundations.js','art.js','components.js','scenarios.js','nc-gauges.js'])vm.runInContext(fs.readFileSync(path.join(__dirname,'..',f),'utf8'),c);
 return c;
}
test('road motion tracks speed proportionally and stops without valid movement',()=>{
 const {NC_COMPONENTS:U}=load();
 const slow=U.roadMotion(40),fast=U.roadMotion(120);
 assert.ok(Math.abs(fast.metersPerSecond/slow.metersPerSecond-3)<1e-9);
 assert.ok(Math.abs(fast.frequency/slow.frequency-3)<1e-9);
 for(const [speed,connected] of [[0,true],[-5,true],[NaN,true],[Infinity,true],[120,false]]){
  const motion=U.roadMotion(speed,connected);assert.equal(motion.moving,false);assert.equal(motion.frequency,0);
 }
 assert.equal(U.roadMotion(1000).speed,220);
});
test('road perspective stays on both sides of the primary readout and wraps continuously',()=>{
 const {NC_COMPONENTS:U}=load();
 for(const side of [-1,1])for(const phase of [0,.1,.4,.7,.9,.99]){
  const p=U.roadProjection(side,phase);
  assert.ok(Number.isFinite(p.x)&&Number.isFinite(p.y)&&Number.isFinite(p.scale));
  assert.ok(p.opacity>=0&&p.opacity<=1);
  // The widest building extends 20 local pixels to each side.
  assert.ok(side<0?p.x+20*p.scale<244:p.x-20*p.scale>476);
  assert.ok(p.y>=372&&p.y<=575);
  if(phase<.98){const next=U.roadProjection(side,phase+.01);assert.ok(next.scale>=p.scale);}
 }
 assert.equal(JSON.stringify(U.roadProjection(-1,.2)),JSON.stringify(U.roadProjection(-1,1.2)));
});
test('preview speed drives both the reading and optional speed-limit reminder',()=>{
 const {NC}=load();
 const state={...NC.defaults,scenario:'speed-limit',previewSpeed:60};
 assert.equal(NC.resolve('speed',state).values.speed,60);assert.equal(NC.resolve('speed',state).alerts.length,0);
 assert.equal(NC.resolve('speed',{...state,previewSpeed:140}).alerts.length,1);
 const parked=NC.render('speed',{...NC.defaults,previewSpeed:0});
 assert.match(parked,/data-road-state="stopped"/);assert.match(parked,/CRUISE OFF/);
 const offline=NC.render('speed',{...state,previewSpeed:140,signal:false});
 assert.match(offline,/data-road-speed="0"/);assert.match(offline,/data-road-frequency="0.000"/);assert.doesNotMatch(offline,/SPEED LIMIT/);
 assert.match(NC.render('speed',NC.defaults),/data-road-kind="tree"/);assert.match(NC.render('speed',NC.defaults),/data-road-kind="building"/);
});
