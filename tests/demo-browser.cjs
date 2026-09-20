// Browser integration checks only: the fake port is injected into an isolated test page.
// Run with NODE_PATH pointing to installed playwright, and tools/serve_demo.py on port 8001.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const out = process.env.DEMO_TEST_OUTPUT || path.join(root,'build/demo-browser');
fs.mkdirSync(out,{recursive:true});

(async()=>{
  const browser = await chromium.launch({channel:'chrome',headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:900}});
  const errors=[];
  page.on('pageerror', e=>errors.push(e.message));
  page.on('response', r=>{if(r.status()>=400) errors.push(`${r.status()} ${r.url()}`);});
  await page.addInitScript(()=>{
    let sink;
    window.testPort={
      async open(settings){window.openSettings=settings; this.readable=new ReadableStream({start(c){sink=c;}});},
      async close(){this.readable=null;},
    };
    Object.defineProperty(navigator,'serial',{value:{requestPort:async()=>window.testPort},configurable:true});
    window.feedTestSerial=text=>sink.enqueue(new TextEncoder().encode(text));
  });
  try {
    await page.goto('http://localhost:8001/demo-visualizer/');
    await page.waitForFunction(()=>!document.querySelector('#connect').disabled);
    await page.waitForFunction(()=>document.querySelector('#idle-video').currentTime>0);
    const media=await page.evaluate(async()=>{
      const manifest=await (await fetch('assets.json')).json();
      const result={images:{},videos:{}};
      for(const [key,url] of Object.entries(manifest.images)) {
        const img=new Image(); img.src=url; await img.decode();
        result.images[key]=[img.naturalWidth,img.naturalHeight];
      }
      for(const [key,url] of Object.entries(manifest.videos)) {
        const video=document.createElement('video'); video.preload='metadata';video.src=url;
        await new Promise((resolve,reject)=>{video.onloadedmetadata=resolve;video.onerror=reject;});
        result.videos[key]={duration:video.duration,width:video.videoWidth,height:video.videoHeight};
        video.removeAttribute('src'); video.load();
      }
      return result;
    });
    for(const size of Object.values(media.images)) assert.deepEqual(size,[1024,768]);
    await page.screenshot({path:path.join(out,'demo-idle.png'),fullPage:true});
    await page.locator('#connect').click();
    assert.deepEqual(await page.evaluate(()=>window.openSettings),{baudRate:115200,dataBits:8,stopBits:1,parity:'none',flowControl:'none'});

    // Replay the actual saved UART record, including its untouched raw ADC samples and results.
    const log=fs.readFileSync(path.join(root,'docs/analysis/2026-09-20-live-calibration/pressure-legs-20260920.uart.log'),'utf8');
    const lines=log.split('\n').map(s=>s.trimEnd()).filter(Boolean);
    const lastResult=lines.findLastIndex(s=>s.startsWith('# PNEC1,'));
    let n=lastResult+1, tailSamples=0;
    while(n<lines.length && tailSamples<15){if(lines[n].startsWith('PNEU1,'))tailSamples++;n++;}
    const replay=lines.slice(0,n).join('\r\n')+'\r\n';
    await page.evaluate(text=>window.feedTestSerial(text),replay);
    const expectedPart=lines[lastResult].split(',')[3];
    await page.waitForFunction(part=>document.querySelector('#part-en').textContent===part,expectedPart);
    await page.waitForFunction(()=>!document.querySelector('#reaction-video').hidden);
    assert.equal(await page.locator('#chart-empty').isVisible(),false);
    assert.match(await page.locator('#trace-detail').innerText(),/samples/);
    await page.screenshot({path:path.join(out,'demo-recorded-waveform.png'),fullPage:true});

    // Clear replay state, then use explicit test-only events to verify every media mapping and timer.
    await page.evaluate(()=>{
      window.feedTestSerial('# PneutouchAi live acquisition resumed; sequence restarts\r\n');
      window.demoSeq=0;window.demoMs=0;
      window.testSample=()=>window.feedTestSerial(`PNEU1,${window.demoSeq++},${window.demoMs+=25},4000000\r\n`);
      window.testStream=setInterval(window.testSample,25);
    });
    for(const [index,part] of ['HEAD','BACK','LEGS','TAIL'].entries()) {
      const id=100+index;
      await page.evaluate(id=>window.feedTestSerial(`# PNEE1,${id},START,${window.demoMs}\r\n`),id);
      await page.waitForFunction(()=>document.querySelector('#status-image').src.endsWith('/pressed.jpg'));
      assert.equal(await page.locator('#status-stage').getAttribute('data-phase'),'pressed');
      if(index===0) await page.screenshot({path:path.join(out,'demo-pressed.png'),fullPage:true});
      await page.evaluate(({id,part})=>window.feedTestSerial(`# PNEC1,${id},${window.demoMs},${part},4700,3f00,3f00,3f00,3f00\r\n`),{id,part});
      await page.waitForFunction(part=>document.querySelector('#status-image').src.endsWith('/'+part.toLowerCase()+'.jpg'),part);
      await page.waitForFunction(part=>!document.querySelector('#reaction-video').hidden && document.querySelector('#reaction-video').src.endsWith('/'+part.toLowerCase()+'.mp4'),part);
      if(index===3){
        const began=Date.now();
        await page.waitForFunction(()=>document.querySelector('#status-stage').dataset.phase==='idle',{},{timeout:4000});
        assert(Date.now()-began>=2600,'image must remain red for about three seconds');
        assert.equal(await page.locator('#reaction-video').isVisible(),true,'video continues after image reset');
        assert.equal(await page.locator('#chart-empty').isVisible(),false,'graph is retained');
        await page.waitForFunction(()=>document.querySelector('#reaction-video').hidden,{},{timeout:3500});
        assert(Date.now()-began>=4400,'reaction lasts about five seconds');
        assert.equal(await page.locator('#idle-video').evaluate(v=>v.loop&&!v.paused),true);
      }
    }
    await page.evaluate(()=>clearInterval(window.testStream));
    await page.locator('#disconnect').click();
    await page.waitForFunction(()=>document.querySelector('#connection').textContent==='未接続');
    assert.equal(await page.locator('#chart-empty').isVisible(),true);
    await page.setViewportSize({width:390,height:844});
    await page.screenshot({path:path.join(out,'demo-mobile.png'),fullPage:true});
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'no horizontal overflow on mobile');
    assert.deepEqual(errors,[]);
    fs.writeFileSync(path.join(out,'browser-check.json'),JSON.stringify({media,checks:['actual saved UART waveform','all four JPEG/video routes','pressed image','3-second image reset','5-second reaction','idle loop','disconnect','mobile width'],errors},null,2));
    console.log(JSON.stringify({ok:true,media,output:out},null,2));
  } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
