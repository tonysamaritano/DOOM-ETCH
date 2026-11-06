import * as THREE from 'three';
import {decodeFrame} from './decoder.js';
import {buildScene} from './scene.js';
const bridge=window.webkit?.messageHandlers?.presentation;
const status=document.getElementById('status'),badge=document.getElementById('physical-mode');
const evidence={state:'loading',assetURL:new URL('rig.glb',location.href).href,
  frameURL:new URL('frame',location.href).href,sequence:0,errors:[]};
let stopped=false,failed=false,request=null,animation=0,rig=null,renderer=null;
function publish(){document.getElementById('play-evidence').textContent=JSON.stringify(evidence);}
function fail(error) {
  if(stopped||failed)return;failed=true;cancelAnimationFrame(animation);request?.abort();
  const message=String(error);evidence.state='error';evidence.errors.push(message);publish();
  if(!document.documentElement.dataset.failed){document.documentElement.dataset.failed='true';
    status.hidden=false;status.className='error';status.textContent=message;bridge?.postMessage({type:'error',message});}
}
function read(name) {
  return new Promise((resolve,reject)=>{
    const xhr=new XMLHttpRequest();request=xhr;
    xhr.open('GET',new URL(name,location.href).href);xhr.responseType='arraybuffer';
    const release=()=>{if(request===xhr)request=null;};
    xhr.onload=()=>{release();if((xhr.status&&xhr.status!==200)||!(xhr.response instanceof ArrayBuffer))reject(Error('Could not read '+name+' (HTTP '+xhr.status+')'));else resolve(xhr.response);};
    xhr.onerror=()=>{release();reject(Error('Could not read '+name));};
    xhr.onabort=()=>{release();reject(Error('Read cancelled: '+name));};xhr.send();
  });
}
window.addEventListener('pagehide',()=>{stopped=true;cancelAnimationFrame(animation);request?.abort();});
async function poll() {
  if(stopped||failed)return;
  try {
    const frame=decodeFrame(await read('frame'));
    if(stopped||failed)return;
    if(frame) {
      const m=frame.metadata;
      if(m.sequence<=evidence.sequence)throw Error('Repeated or stale delivered sequence');
      const motion=rig.apply(frame);rig.render();
      if(failed||stopped)return;
      Object.assign(evidence,{state:'presenting',sequence:m.sequence,mode:m.mode,kind:m.kind,
        width:m.width,height:m.height,physical_seconds:m.physical_seconds??null,
        cursor_mm:m.cursor_mm??null,motor_mm:m.motor_mm??null,travel_mm:m.travel_mm??null,
        motion,canvas:{width:renderer.domElement.width,height:renderer.domElement.height},scene:rig.evidence()});
      badge.hidden=m.mode!=='physical';
      badge.textContent=m.mode==='physical'?'Physical · 1× · '+m.physical_seconds.toFixed(3)+' s':'';
      status.hidden=true;publish();bridge.postMessage({type:'presented',sequence:m.sequence});
    }
    animation=requestAnimationFrame(()=>poll().catch(fail));
  } catch(error){fail(error);}
}
function resize() {
  if(!rig||failed||stopped)return;
  try{rig.resize(innerWidth,innerHeight);rig.render();if(!failed){evidence.scene=rig.evidence();publish();}}
  catch(error){fail(error);}
}
window.addEventListener('resize',resize);
async function start() {
  if(!bridge)throw Error('Native presentation bridge is unavailable');
  renderer=new THREE.WebGLRenderer({antialias:true,alpha:false});
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.outputColorSpace=THREE.SRGBColorSpace;
  renderer.toneMapping=THREE.AgXToneMapping;renderer.toneMappingExposure=1.05;
  document.body.prepend(renderer.domElement);
  renderer.domElement.addEventListener('webglcontextlost',()=>fail(Error('WebGL context lost')));
  renderer.debug.onShaderError=(gl,program,vertex,fragment)=>{
    const detail=[gl.getProgramInfoLog(program),gl.getShaderInfoLog(vertex),gl.getShaderInfoLog(fragment)].filter(Boolean).join('\n');
    fail(Error('Rig shader failure: '+detail.slice(0,1200)));
  };
  rig=await buildScene(renderer,read);
  if(failed||stopped||document.documentElement.dataset.failed)return;
  resize();if(failed||stopped)return;
  if(!renderer.info.render.calls)throw Error('Rig scene did not render');
  evidence.state='ready';evidence.scene=rig.evidence();publish();
  bridge.postMessage({type:'ready'});animation=requestAnimationFrame(()=>poll().catch(fail));
}
start().catch(fail);
