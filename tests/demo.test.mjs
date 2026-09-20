import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {parseDemoLine, PARTS} from '../src/demo-visualizer/protocol.mjs';
import {DemoState} from '../src/demo-visualizer/state.mjs';
import {VideoPlayback} from '../src/demo-visualizer/media.mjs';
const result = (id,part='HEAD',ms=1600) => parseDemoLine(`# PNEC1,${id},${ms},${part},4717,3f00,3f00,3f00,3f00\r\n`);
const start = (id,ms) => ({type:'event',action:'START',id,ms});
const sample = (seq,ms,raw=4000000) => ({type:'sample',seq,ms,raw});
test('all live protocol types, classes, malformed fields and counter bounds', () => {
  for (const part of [...PARTS,'UNKNOWN']) assert.equal(result(1,part).part,part);
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
  const p=new VideoPlayback(idle,react,{static:'static.mp4',head:'head.mp4',back:'back.mp4',legs:'legs.mp4',tail:'tail.mp4'},{timers});
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
