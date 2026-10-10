const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const dir=path.resolve(__dirname,'..');
function load(){
  const context=vm.createContext({});
  for(const file of ['foundations.js','art.js','components.js','nc-gauges.js'])vm.runInContext(fs.readFileSync(path.join(dir,file),'utf8'),context);
  return context;
}
test('shared foundations exports stay synchronized with source tokens',()=>{
  const context=load();
  assert.equal(JSON.stringify(context.NC_FOUNDATIONS),JSON.stringify(JSON.parse(fs.readFileSync(path.join(dir,'foundations.json'),'utf8'))));
  assert.equal(context.NC_FOUNDATIONS.geometry.master/context.NC_FOUNDATIONS.geometry.display,2);
});
test('hit areas reject undersized, non-finite, and clipped corners',()=>{
  const U=load().NC_COMPONENTS;
  assert.equal(U.validateHitRegion({x:316,y:316,width:88,height:88}),true);
  assert.throws(()=>U.validateHitRegion({x:316,y:316,width:86,height:88}),/44×44/);
  assert.throws(()=>U.validateHitRegion({x:600,y:600,width:88,height:88}),/safe area/);
  assert.throws(()=>U.validateHitRegion({x:NaN,y:316,width:88,height:88}),/finite/);
});
test('readout state communicates warning and unavailable data',()=>{
  const context=load(),U=context.NC_COMPONENTS,props={x:200,y:220,label:'OIL TEMP',value:'104',unit:'°C'};
  const warningFill='fill="'+context.NC_FOUNDATIONS.colors.red+'"';
  assert.ok(U.readout({...props,state:'warning'}).includes(warningFill));
  assert.ok(U.readout({...props,state:'warning',color:'green'}).includes(warningFill));
  const unavailable=U.readout({...props,state:'disconnected'});
  assert.match(unavailable,/>—<\/text>/);
  assert.doesNotMatch(unavailable,/>104<\/text>/);
  assert.throws(()=>U.readout({...props,size:'unknown'}),/Unknown Readout size/);
  assert.throws(()=>U.readout({...props,size:'__proto__'}),/Unknown Readout size/);
  assert.throws(()=>U.readout({...props,x:NaN}),/finite/);
  assert.throws(()=>U.readout({...props,value:undefined}),/needs a value/);
  assert.match(U.readout({...props,value:0}),/>0<\/text>/);
});
test('disabled actions remove keyboard focus and carry an explicit state',()=>{
  const U=load().NC_COMPONENTS;
  const svg=U.actionButton({x:240,y:260,width:240,label:'CONNECT',disabled:true});
  assert.match(svg,/tabindex="-1"/);
  assert.match(svg,/aria-disabled="true"/);
});
test('all pages and important states obey component geometry contracts',()=>{
  const NC=load().NC;
  assert.equal(NC.pages.length,16);
  const states=[NC.defaults,{...NC.defaults,alarm:true},{...NC.defaults,signal:false},{...NC.defaults,night:true},{...NC.defaults,dialog:true},{...NC.defaults,day:100,dark:10}];
  for(const page of NC.pages)for(const state of states){
    const svg=NC.render(page.id,state);
    assert.match(svg,/viewBox="0 0 720 720"/);
    assert.doesNotMatch(svg,/undefined|NaN|__NC_COLOR_|__NC_FONT_/);
  }
});
test('artwork uses the same semantic accent as composed SVG components',()=>{
  const context=vm.createContext({});
  vm.runInContext(fs.readFileSync(path.join(dir,'foundations.js'),'utf8'),context);
  context.NC_FOUNDATIONS={...context.NC_FOUNDATIONS,colors:{...context.NC_FOUNDATIONS.colors,amber:'#ABC123'}};
  for(const file of ['art.js','components.js'])vm.runInContext(fs.readFileSync(path.join(dir,file),'utf8'),context);
  const svg=context.NC_COMPONENTS.art('roadster')+context.NC_COMPONENTS.liveArc(360,360,246,135,300);
  assert.match(svg,/#ABC123/);
  assert.doesNotMatch(svg,/#FFB35B/);
});
