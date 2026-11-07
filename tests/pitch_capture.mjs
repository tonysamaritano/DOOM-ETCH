// SPDX-License-Identifier: GPL-2.0-or-later
import assert from 'node:assert/strict';
import test from 'node:test';
import {startCapture} from '../viewer/capture.js';
async function run(options={}) {
  const original=globalThis.fetch,requests=[],times=[],progress=[];let pending=false;
  const canvas={width:0,height:0,toBlob(callback,mime,quality){
    assert.equal(mime,'image/jpeg');assert.equal(quality,.94);
    callback(options.blob===undefined?new Blob(['encoder-fixture'],{type:mime}):options.blob);
  }};
  globalThis.fetch=async (path,init)=>{
    assert.equal(pending,false);pending=true;assert.equal(init.method,'POST');
    const url=new URL(path,'http://localhost'),frame=Number(url.searchParams.get('frame'));
    requests.push(path);await new Promise(resolve=>setImmediate(resolve));pending=false;
    if(url.pathname==='/__frame'&&frame===options.failFrame)return {ok:false,status:500};
    const ack={ok:true,cut:'pitch',token:'session-token'};
    if(url.pathname==='/__frame') {
      assert.equal(init.headers['Content-Type'],'image/jpeg');assert.ok(init.body.size>0);
      ack.frame=frame;if(frame===options.wrongFrame)ack.frame++;
      if(frame===options.wrongToken)ack.token='stale';
    }
    if(url.pathname==='/__done')ack.frames=options.doneFrames??540;
    return {ok:true,json:async()=>ack};
  };
  try {
    const result=await startCapture({canvas,setSize(w,h){canvas.width=w;canvas.height=h;},
      renderAt(t){assert.equal(pending,false);times.push(t);if(options.resize)canvas.width=1;},
      onProgress:p=>progress.push(p)});
    return {result,requests,times,progress};
  } catch(error) {return {error,requests,times,progress};}
  finally {globalThis.fetch=original;}
}
test('capture awaits all 540 exact samples and verified completion',async()=>{
  const value=await run();assert.equal(value.error,undefined);
  assert.deepEqual(value.result,{frames:540});assert.equal(value.requests.length,542);
  assert.deepEqual(value.times,Array.from({length:540},(_,i)=>i/30));
  assert.deepEqual(value.progress.filter(p=>p.state==='capturing').map(p=>p.completed),
    Array.from({length:540},(_,i)=>i+1));
  assert.equal(value.progress.at(-1).state,'done');
  assert.equal(value.progress.at(-1).completed,540);
});
test('HTTP failure never acknowledges or renders a later frame',async()=>{
  const value=await run({failFrame:7});assert.match(value.error.message,/HTTP 500/);
  assert.equal(value.times.length,8);assert.equal(value.progress.at(-1).completed,7);
  assert.equal(value.progress.at(-1).state,'error');
  assert.ok(!value.requests.some(p=>p.startsWith('/__done')));
  assert.ok(!value.progress.some(p=>p.state==='done'));
});
test('bad encoding, stale acknowledgments and resizing fail closed',async()=>{
  for(const options of [{blob:null},{blob:new Blob([],{type:'image/jpeg'})},
    {blob:new Blob(['x'],{type:'image/png'})},{wrongFrame:2},{wrongToken:2},{resize:true}]) {
    const value=await run(options);assert.ok(value.error);
    assert.equal(value.progress.at(-1).state,'error');
    assert.ok(!value.progress.some(p=>p.state==='done'));
    assert.ok(!value.requests.some(p=>p.startsWith('/__done')));
  }
});
test('incomplete final acknowledgment cannot announce completion',async()=>{
  const value=await run({doneFrames:539});assert.equal(value.times.length,540);
  assert.match(value.error.message,/Incomplete/);assert.equal(value.progress.at(-1).state,'error');
  assert.ok(!value.progress.some(p=>p.state==='done'));
});
