const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.join(__dirname,'..');
function load(){const c=vm.createContext({});for(const file of ['foundations.js','art.js','components.js','scenarios.js','nc-gauges.js'])vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),c);return c;}
const expected={valuePrimary:180,valueCompact:80,valueSecondary:50,pairCode:80,contentTitle:60,pageTitle:36,label:32,action:32,status:32,content:32,unit:28,scale:28,meta:28};
const attrs=s=>Object.fromEntries([...s.matchAll(/([\w-]+)="([^"]*)"/g)].map(m=>[m[1],m[2]]));
function verify(svg,roles,where){
 const nodes=[...svg.matchAll(/<text\b([^>]*)>([^<]*)<\/text>/g)];assert.ok(nodes.length,where);
 for(const [,raw,text] of nodes){const a=attrs(raw),r=roles[a['data-type-role']];assert.ok(r,where+': '+text);assert.equal(+a['font-size'],r.size,where+': '+text);assert.equal(a['font-family'],r.family);assert.equal(+a['font-weight'],r.weight);assert.equal(+a['letter-spacing'],r.letterSpacing);assert.ok(!a.style&&!a.textLength&&!a['font-stretch'],'No per-page typography compression');}
}
test('all instruments and scenarios use complete semantic type definitions',()=>{
 const {NC,NC_FOUNDATIONS:F}=load();assert.equal(Object.keys(F.typography.roles).length,13);
 for(const [name,size] of Object.entries(expected))assert.equal(F.typography.roles[name].size,size,name);
 for(const p of NC.pages)for(const scene of NC.scenarios[p.id])for(const signal of [true,false])for(const night of [false,true])verify(NC.render(p.id,{...NC.defaults,scenario:scene.id,signal,night}),F.typography.roles,p.id+'/'+scene.id);
 for(const [id,dialog] of [['obd','obd'],['pair','phone'],['bluetooth','obd']])verify(NC.render(id,{...NC.defaults,dialog}),F.typography.roles,id+'/dialog');
});
test('readout geometry cannot change the meaning or type size of labels and units',()=>{
 const {NC_COMPONENTS:U,NC_FOUNDATIONS:F}=load();
 for(const size of ['primary','compact','eccentric','analog'])for(const state of ['normal','warning','disconnected']){
  const svg=U.readout({x:200,y:220,label:'OIL TEMP',value:104,unit:'°C',size,state});verify(svg,F.typography.roles,size);
  const nodes=[...svg.matchAll(/<text\b([^>]*)>([^<]*)<\/text>/g)].map(m=>attrs(m[1]));
  assert.equal(nodes[0]['data-type-role'],'label');assert.equal(nodes[2]['data-type-role'],'unit');assert.equal(nodes[1]['data-type-role'],size==='compact'?'valueCompact':'valuePrimary');
 }
 assert.throws(()=>U.typeText({x:0,y:0,text:'test',role:'tiny'}),/Unknown typography/);
 assert.throws(()=>U.typeText({x:NaN,y:0,text:'test',role:'label'}),/finite/);
 assert.match(U.typeText({x:0,y:0,text:'test',role:'label',size:1,weight:100}),/font-size="32" font-weight="500"/);
});
test('firmware opt-in semantic metadata matches the prototype without changing legacy roles',()=>{
 const {NC_FOUNDATIONS:F}=load(),tokens=fs.readFileSync(path.join(root,'../ui/JDM/core/jdm_tokens.h'),'utf8'),logic=fs.readFileSync(path.join(root,'../ui/JDM/core/jdm_fonts_logic.h'),'utf8');
 for(const [role,spec] of Object.entries(F.typography.roles)){
  const name=role.replace(/([a-z])([A-Z])/g,'$1_$2').toUpperCase();
  assert.equal(+tokens.match(new RegExp('#define JDM_TYPE_'+name+'\\s+(\\d+)'))[1],spec.size);
  const family=spec.family===F.typography.number?(spec.weight===600?'BARLOW_SEMI':'BARLOW'):'MONO_MED';
  assert.ok(logic.includes('[JDM_TYPE_ROLE_'+name+'] = {JDM_FAM_'+family+', JDM_TYPE_'+name+'}'));
 }
 const legacy=fs.readFileSync(path.join(root,'../ui/JDM/core/jdm_fonts.c'),'utf8');
 assert.match(legacy,/JDM_FONT_SMALL\]\s*= \{JDM_FAM_MONO,\s+22\}/);assert.match(legacy,/JDM_FONT_LABEL\]\s*= \{JDM_FAM_MONO_MED,\s+26\}/);
});
test('measurement units are separate from measurement and control values',()=>{
 const {NC}=load();
 for(const id of ['gear','tyres','speed','display','multi']){
  const svg=NC.render(id,NC.defaults);
  for(const [,raw,text] of svg.matchAll(/<text\b([^>]*)>([^<]*)<\/text>/g)){if(attrs(raw)['data-type-role']?.startsWith('value'))assert.doesNotMatch(text,/%|°C|bar|RPM|km\/h/);}
 }
});
