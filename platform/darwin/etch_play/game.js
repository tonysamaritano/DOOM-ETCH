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
