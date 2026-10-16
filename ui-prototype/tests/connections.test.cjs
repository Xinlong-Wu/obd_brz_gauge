const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
function load(){
 const context=vm.createContext({});
 for(const file of ['foundations.js','art.js','components.js','scenarios.js','nc-gauges.js'])vm.runInContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),context);
 return context;
}
test('OBD discovery commits a replacement only after the vehicle data stage succeeds',()=>{
 const {NC,NC_SCENARIOS:S}=load();
 let s={...NC.defaults,paired:true};
 s=S.connectionAction(s,'scan-obd');
 assert.equal(s.scenarios.obd,'scanning');
 s=S.connectionAction(s,'advance-obd');assert.equal(s.scenarios.obd,'normal');
 s=S.connectionAction(s,'select-obd-1');assert.equal(s.scenarios.obd,'connecting');
 assert.equal(s.obdDevice,'OBDLink CX');
 const cancelled=S.connectionAction(s,'cancel-obd');
 assert.equal(cancelled.obdDevice,'OBDLink CX');assert.equal(cancelled.scenarios.obd,'normal');
 s=S.connectionAction(s,'advance-obd');assert.equal(s.scenarios.obd,'checking');
 s=S.connectionAction(s,'advance-obd');assert.equal(s.scenarios.obd,'connected');
 assert.equal(s.obdDevice,'Vgate iCar Pro');assert.equal(s.paired,true);
 assert.equal(NC.defaults.obdDevice,'OBDLink CX');
});
test('forgetting OBD or phone preserves the other connection and remote setting',()=>{
 const {NC,NC_SCENARIOS:S}=load(),initial={...NC.defaults,paired:true};
 let s=S.connectionAction(initial,'forget-obd');
 assert.match(NC.render('obd',s),/Forget OBD adapter/);
 s=S.connectionAction(s,'clear');
 assert.equal(s.obdBound,false);assert.equal(s.paired,true);assert.equal(s.keyboard,true);
 assert.equal(s.scenarios.obd,'scanning');
 s=S.connectionAction(initial,'forget-phone');
 assert.match(NC.render('pair',s),/Forget phone/);
 s=S.connectionAction(s,'clear');
 assert.equal(s.paired,false);assert.equal(s.obdBound,true);assert.equal(s.keyboard,true);
 assert.equal(S.connectionAction(S.connectionAction(initial,'forget-obd'),'cancel').obdBound,true);
});
test('phone pairing and OBD loss remain separate recoverable states',()=>{
 const {NC,NC_SCENARIOS:S}=load();
 let s={...NC.defaults,scenarios:{bluetooth:'connection-lost',pair:'pair-expired'}};
 s=S.connectionAction(s,'retry-pair');
 assert.equal(s.pairCode,'2468 1357');assert.equal(s.scenarios.bluetooth,'connection-lost');
 s=S.connectionAction(s,'confirm-pair');
 assert.equal(s.paired,true);assert.equal(s.scenarios.bluetooth,'connection-lost');
 s=S.connectionAction(s,'obd');assert.equal(s.scenarios.obd,'disconnected');
 s=S.connectionAction(s,'retry-obd');assert.equal(s.scenarios.obd,'connecting');
 const hidden=NC.render('pair',{...NC.defaults,visible:false});
 assert.match(hidden,/NOT DISCOVERABLE/);assert.match(hidden,/aria-disabled="true"/);
 assert.doesNotMatch(hidden,/>1234 5678<\/text>/);
 for(const id of ['bluetooth','pair','obd']){
  assert.equal(NC.resolve(id,{...NC.defaults,signal:false}).vehicle,false);
  assert.doesNotMatch(NC.render(id,{...NC.defaults,signal:false}),/NO SIGNAL/);
 }
});
test('OBD waiting, empty, failed and lost scenes expose the right recovery controls',()=>{
 const {NC}=load();
 for(const [scenario,copy,act] of [
  ['vehicle-off','Switch on the ignition','scan-obd'],
  ['no-devices','NO ADAPTER FOUND','scan-obd'],
  ['connect-failed','CONNECTION FAILED','retry-obd'],
  ['disconnected','Saved adapter retained','retry-obd']
 ]){
  const svg=NC.render('obd',{...NC.defaults,scenario});
  assert.ok(svg.includes(copy));assert.ok(svg.includes('data-act="'+act+'"'));
  assert.match(svg,/data-act="connections"/);assert.doesNotMatch(svg,/PAIRING CODE/);
 }
});
