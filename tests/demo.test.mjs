import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {parseDemoLine, ALL_PARTS, KNOWN_PARTS} from '../src/demo-visualizer/protocol.mjs';
import {DemoState} from '../src/demo-visualizer/state.mjs';
import {VideoPlayback} from '../src/demo-visualizer/media.mjs';
import {validateAssets} from '../src/demo-visualizer/assets.mjs';
const result = (id,part='HEAD',ms=1600) => parseDemoLine(`# PNEC1,${id},${ms},${part},4717,3f00,3f00,3f00,3f00\r\n`);
const start = (id,ms) => ({type:'event',action:'START',id,ms});
const sample = (seq,ms,raw=4000000) => ({type:'sample',seq,ms,raw});
test('all live protocol types, classes, malformed fields and counter bounds', () => {
  for (const part of [...ALL_PARTS,'UNKNOWN']) assert.equal(result(1,part).part,part);
  assert.equal(parseDemoLine('# PNEE1,1,START,123\r\n').action,'START');
  assert.deepEqual(parseDemoLine('# PNEF1,1,100,200,6,1000,2000,3000,4000,5000,6000\n').values,[1,2,3,4,5,6]);
  assert.equal(parseDemoLine('# PneutouchAi live acquisition resumed; sequence restarts\n').type,'reset');
  for (const line of ['# PNEE1,-1,START,10','# PNEC1,1,2,NECK,4,3f00,3f00,3f00,3f00',
    '# PNEE1,4294967296,START,10','# PNEF1,1,2,3,2,1,2,3,4,5,6',
    '# PNEC1,1,2,HEAD,NaN,3f00,3f00,3f00,3f00']) assert.equal(parseDemoLine(line),null);
});
test('press is unknown/yellow; exact event samples arrive after prediction; red resets at 3 seconds', () => {
  const m=new DemoState();
  for (let i=0;i<41;i++) m.consume(sample(i,i*25),i*25);
  m.consume(start(1,1000),1000); assert.equal(m.phase,'pressed'); assert.equal(m.part,null);
  for(let i=41;i<61;i++) m.consume(sample(i,i*25,4000000+Math.round(100000*Math.sin(i))),i*25);
  m.consume(result(1),1600); assert.equal(m.phase,'result'); assert.equal(m.part,'HEAD');
  m.consume({type:'features',id:1,trigger:1000,end:1500,offset:0,values:[1,2,3,4,5,6]},1650);
  assert.equal(m.trace.exactBounds,true); assert.equal(m.trace.base,4000000);
  assert(m.trace.points.some(p=>p.raw!==4000000));
  assert.equal(m.trace.partial,false); assert.equal(m.tick(4599),false);
  assert.equal(m.tick(4600),true); assert.equal(m.phase,'idle'); assert.equal(m.trace.part,'HEAD');
});
test('new touch supersedes old image timer, duplicate/late results do not replay', () => {
  const m=new DemoState(); m.consume(start(1,100),0); m.consume(result(1),1000);
  assert.deepEqual(m.consume(result(1),1500),[]);
  m.consume(start(2,200),2500); m.tick(4000); assert.equal(m.phase,'pressed');
  assert.deepEqual(m.consume(result(1),4200),[]); assert.equal(m.phase,'pressed');
  m.consume({type:'event',action:'INVALID',id:1,ms:300},4300); assert.equal(m.phase,'pressed');
  m.consume({type:'event',action:'TIMEOUT',id:2,ms:400},4400); assert.equal(m.phase,'idle');
});

test('waveform appears only on a new result for five seconds, independently of image and late features', () => {
  const m=new DemoState();
  assert.equal(m.traceDeadline,0);
  m.consume(sample(0,1000),0); m.consume(start(1,1000),0);
  assert.equal(m.traceDeadline,0);
  m.consume(result(1),1000); assert.equal(m.traceDeadline,6000);
  m.tick(4000); assert.equal(m.phase,'idle'); assert.equal(m.traceDeadline,6000);
  m.consume(result(1),5000); // Duplicate result must not prolong the display.
  assert.equal(m.tick(5999),false);
  assert.equal(m.tick(6000),true); assert.equal(m.traceDeadline,0);
  m.consume({type:'features',id:1,trigger:1000,end:1500,offset:0,values:[1,2,3,4,5,6]},6100);
  assert.equal(m.traceDeadline,0); // Late samples/features cannot make it reappear.
  m.consume(start(2,2000),6200); assert.equal(m.traceDeadline,0);
  m.consume(result(2,'BACK'),6500); assert.equal(m.traceDeadline,11500);
  m.consume(start(3,3000),8000); m.consume(result(3,'TAIL'),8500);
  m.tick(11500); assert.equal(m.traceDeadline,13500); assert.equal(m.trace.part,'TAIL');
  m.tick(13500); assert.equal(m.traceDeadline,0);
  m.reset(); assert.equal(m.traceDeadline,0);
});
test('uint32 wrap remains continuous; a sample gap or MCU reboot clears stale state', () => {
  const m=new DemoState(); m.consume(sample(0xffffffff,0xfffffff0),0);
  assert.deepEqual(m.consume(sample(0,9),25),[]);
  m.consume(start(5,9),25); assert.deepEqual(m.consume(sample(2,59),75),['reset']);
  assert.equal(m.phase,'idle'); assert.equal(m.samples.length,1);
  m.consume({type:'reset'},100); assert.equal(m.samples.length,0);
});
test('object, slot switch and store reply lines', () => {
  assert.deepEqual(KNOWN_PARTS.KRAG,['HAT','FACE']);
  assert.deepEqual(parseDemoLine('# PNEM3,READY,slot=2,count=141,err=0,object=KRAG,sel=1:2:3,fisher=10:20\r\n'),
    {type:'object',object:'KRAG',slot:2,count:141});
  assert.deepEqual(parseDemoLine('# PNEM3,READY,slot=0,count=221,err=0,sel=1:2,fisher=1:2\n'),
    {type:'object',object:'DINO',slot:0,count:221}); // older firmware: no object field
  assert.equal(parseDemoLine('# PNEM3,ERROR,slot=1,count=0,err=3,object=XXXX,sel=,fisher=0\n').object,'DINO');
  assert.deepEqual(parseDemoLine('# PNES1,slot=3,count=40\r\n'),{type:'slot-switch',slot:3,count:40});
  assert.deepEqual(parseDemoLine('# PNES1,ERROR,NO_OTHER_SLOT\r\n'),{type:'slot-error',why:'NO_OTHER_SLOT'});
  assert.deepEqual(parseDemoLine('# PST1,INFO,7,2,221,0,141,0,ss6-thresh\r\n'),
    {type:'store',family:'PST1',op:'INFO',fields:['7','2','221','0','141','0','ss6-thresh'],text:'# PST1,INFO,7,2,221,0,141,0,ss6-thresh'});
  assert.deepEqual(parseDemoLine('# PST1,ERROR,MODE_REQUIRED\n').fields,['MODE_REQUIRED']);
  assert.equal(parseDemoLine('# PAI1,ERROR,CRC\n').op,'ERROR');
  assert.deepEqual(parseDemoLine('# PAI1,MODE,1\r\n'),{type:'mode',mode:1,text:'# PAI1,MODE,1'});
  assert.equal(parseDemoLine('# PAI1,MODE,0,IDLE_TIMEOUT\n').mode,0);
  for (const part of ['HAT','FACE']) assert.equal(result(1,part).part,part);
  for (const line of ['# PNEM4,k=4,mask=0f00','# PNEM5,detect=1:2,object=KRAG','# PNEM1,READY,1,2,3',
    '# PNES1,slot=x,count=1','# PNEM3,READY,slot=1','# PST1','# PAI1,MODE,2']) assert.equal(parseDemoLine(line),null,line);
});
test('object switch resets phase and trace; same object only updates slot/count', () => {
  const m=new DemoState(); assert.equal(m.object,'DINO');
  m.consume(sample(1,1000),0); m.consume(start(1,1000),0); m.consume(result(1),100);
  assert.equal(m.phase,'result'); assert(m.trace);
  assert.deepEqual(m.consume({type:'object',object:'DINO',slot:0,count:221},200),['slot']);
  assert.equal(m.phase,'result'); assert.deepEqual([m.slot,m.count],[0,221]);
  assert.deepEqual(m.consume({type:'object',object:'KRAG',slot:2,count:141},300),['object']);
  assert.equal(m.object,'KRAG'); assert.equal(m.phase,'idle'); assert.equal(m.trace,null);
  assert.deepEqual([m.slot,m.count],[2,141]);
  m.reset(); assert.equal(m.object,'KRAG'); // stream resets keep the object
});
test('a part of another object is unknown; the current object part is a result', () => {
  const m=new DemoState(); m.consume(start(1,100),0);
  assert.deepEqual(m.consume(result(1,'HAT'),100),['result']);
  assert.equal(m.phase,'unknown'); assert.equal(m.part,'UNKNOWN');
  m.consume({type:'object',object:'KRAG',slot:2,count:141},200);
  m.consume(start(2,300),300); m.consume(result(2,'HAT'),400);
  assert.equal(m.phase,'result'); assert.equal(m.part,'HAT');
  m.consume(start(3,500),500); m.consume(result(3,'HEAD'),600); assert.equal(m.phase,'unknown');
});
test('MODE 1 clears the state like a reset; MODE 0 leaves it to the resumed line', () => {
  const m=new DemoState(); m.consume(sample(1,1000),0);
  assert.deepEqual(m.consume(parseDemoLine('# PAI1,MODE,0\n'),10),[]); assert.equal(m.samples.length,1);
  assert.deepEqual(m.consume(parseDemoLine('# PAI1,MODE,1\n'),20),['reset']); assert.equal(m.samples.length,0);
});
test('unknown result preserves raw graph but never claims a body part; mid-event traces marked partial', () => {
  const m=new DemoState(); m.consume(sample(1,1000),0); m.consume(result(2,'UNKNOWN',1000),100);
  assert.equal(m.phase,'unknown'); assert.equal(m.trace.partial,true); assert.equal(m.trace.base,null);
});
test('existing real UART capture supplies event association and exact 12-feature bounds', () => {
  const log=fs.readFileSync(new URL('../docs/analysis/2026-09-20-live-calibration/pressure-legs-20260920.uart.log',import.meta.url),'utf8');
  const m=new DemoState(); let raw=0,starts=0,results=0;
  for(const line of log.split('\n')) {
    // The Windows text capture contains CRCRLF. Normalize the saved fixture only.
    const p=parseDemoLine(line.trimEnd()+'\n');
    if(p?.type==='sample') raw++;
    if(p?.type==='event' && p.action==='START') starts++;
    if(m.consume(p,raw*25).includes('result')) results++;
  }
  assert(raw>4000); assert(starts>=17); assert.equal(results,17);
  assert(m.trace.points.length>20); assert.equal(m.trace.exactBounds,true);
});
class FakeVideo {
  constructor(){this.handlers={};this.hidden=false;this.currentTime=0;this.paused=true;this.playCalls=0;}
  addEventListener(name,fn){this.handlers[name]=fn;}
  play(){this.paused=false;this.playCalls++;return Promise.resolve();}
  pause(){this.paused=true;}
}
function mediaSetup(){
  const jobs=new Map();let id=0;
  const timers={setTimeout:(fn,ms)=>{jobs.set(++id,{fn,ms});return id;},clearTimeout:id=>jobs.delete(id)};
  const idle=new FakeVideo(),react=new FakeVideo();
  const p=new VideoPlayback(idle,react,{static:'static.mp4',HEAD:'head.mp4',BACK:'back.mp4',LEGS:'legs.mp4',TAIL:'tail.mp4'},{timers});
  return {p,idle,react,jobs};
}
test('reaction runs for 5 seconds independently of image state, then returns to looping static',async()=>{
  const {p,idle,react,jobs}=mediaSetup(); await p.showIdle(); await p.play('HEAD');
  assert.equal(react.src,'head.mp4'); assert.equal(idle.hidden,true); assert.equal(idle.loop,true);
  const job=[...jobs.values()][0]; assert.equal(job.ms,5000); job.fn(); await Promise.resolve();
  assert.equal(p.part,'static'); assert.equal(react.hidden,true); assert.equal(idle.paused,false);
});
test('latest inference replaces a reaction; old timeout cannot cancel it; early end returns to static',async()=>{
  const {p,react,jobs}=mediaSetup(); await p.play('HEAD'); const old=[...jobs.values()][0].fn;
  await p.play('TAIL'); old(); assert.equal(p.part,'TAIL'); assert.equal(react.src,'tail.mp4');
  react.handlers.ended(); assert.equal(p.part,'static');
});
test('slow obsolete play completion does not overwrite a newer reaction',async()=>{
  const {p,react}=mediaSetup(); let finish;
  react.play=()=>new Promise(resolve=>{finish=resolve;}); const first=p.play('HEAD');
  react.play=()=>Promise.resolve(); await p.play('LEGS'); finish(); await first;
  assert.equal(p.part,'LEGS'); assert.equal(react.src,'legs.mp4');
});
test('setObject swaps the idle clip and reaction urls, shows idle and cancels a pending reaction',async()=>{
  const {p,idle,react,jobs}=mediaSetup(); let finish;
  react.play=()=>new Promise(resolve=>{finish=resolve;}); const pending=p.play('HEAD');
  react.play=()=>Promise.resolve();
  await p.setObject({static:'krag_static.mp4',HAT:'krag_hat.mp4',FACE:'krag_face.mp4'});
  assert.equal(idle.src,'krag_static.mp4'); assert.equal(p.part,'static'); assert.equal(idle.hidden,false);
  finish(); await pending; assert.equal(p.part,'static'); assert.equal(react.hidden,true); assert.equal(jobs.size,0);
  await p.play('HEAD'); assert.equal(p.part,'static'); // DINO clips are gone
  await p.play('HAT'); assert.equal(react.src,'krag_hat.mp4');
});
test('assets.json lists both objects; validation names the missing asset', () => {
  const assets=JSON.parse(fs.readFileSync(new URL('../src/demo-visualizer/assets.json',import.meta.url),'utf8'));
  assert.equal(validateAssets(assets),assets);
  assert.equal(assets.objects.KRAG.images,null); assert.equal(assets.objects.KRAG.label,'からあげ');
  assert.deepEqual(assets.objects.KRAG.parts,[...KNOWN_PARTS.KRAG]);
  for (const file of Object.values(assets.objects.KRAG.videos)) assert(fs.existsSync(new URL('..'+file,import.meta.url)),file);
  const without=(obj,key,drop)=>({objects:{...assets.objects,[obj]:{...assets.objects[obj],[key]:drop(assets.objects[obj][key])}}});
  assert.throws(()=>validateAssets(without('KRAG','videos',({FACE,...rest})=>rest)),/KRAG.*FACE/);
  assert.throws(()=>validateAssets(without('DINO','images',({pressed,...rest})=>rest)),/DINO.*pressed/);
  assert.throws(()=>validateAssets(without('DINO','images',()=>null)),/DINO/);
  assert.throws(()=>validateAssets(without('KRAG','names',({HAT,...rest})=>rest)),/KRAG.*HAT/);
  assert.throws(()=>validateAssets({objects:{DINO:assets.objects.DINO}}),/KRAG/);
  assert.throws(()=>validateAssets(null),/objects/);
});
