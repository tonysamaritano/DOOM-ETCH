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
