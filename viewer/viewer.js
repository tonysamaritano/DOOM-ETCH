import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {buildStore} from './store.js';
import {buildCamera} from './camera.js';
import {validateTrace,sampleTrace} from './trace.js';
import {buildInk} from './ink.js';
import {buildDrives} from './drives.js';
import {buildTransport} from './transport.js';
const $=id=>document.getElementById(id),stage=$('stage');
const evidence={state:'loading',assetURL:new URL('../assets/rig/etch-rig.glb',location.href).href,
  traceURL:null,physical_time:0,errors:[]};
function publish(){evidence.canvas={width:renderer.domElement.width,height:renderer.domElement.height};
  $('viewer-evidence').textContent=JSON.stringify(evidence);}
const renderer=new THREE.WebGLRenderer({antialias:true,preserveDrawingBuffer:true});
renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.outputColorSpace=THREE.SRGBColorSpace;
renderer.toneMapping=THREE.AgXToneMapping;renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;
stage.prepend(renderer.domElement);
const scene=new THREE.Scene();buildStore(scene,renderer);
const optics=buildCamera(scene,renderer,stage);let runtime=null;
function fail(error){evidence.state='error';evidence.errors.push(String(error));$('status').textContent=String(error);
  $('notice').hidden=false;$('notice').textContent='Could not load this drawing. Check its trace URL and local assets.';publish();}
async function loadRig(){
  const response=await fetch(evidence.assetURL);if(!response.ok)throw Error('Rig asset HTTP '+response.status);
  const gltf=await new GLTFLoader().parseAsync(await response.arrayBuffer(),'');
  const model=gltf.scene;model.traverse(o=>{if(o.isMesh){o.castShadow=true;o.receiveShadow=true;}});scene.add(model);
  const area=model.getObjectByName('DrawArea');if(!area||area.parent?.name!=='Screen')throw Error('Missing Screen/DrawArea contract');
  evidence.assetLoaded=true;evidence.nodes=gltf.parser.json.nodes.length;optics.setModel(model);return {model,area};
}
async function loadDrawing(rig) {
  const url=new URLSearchParams(location.search).get('trace');
  if(!url){evidence.state='awaiting-trace';publish();return;}
  evidence.traceURL=new URL(url,location.href).href;
  const response=await fetch(evidence.traceURL,{cache:'no-store'});if(!response.ok)throw Error('Trace HTTP '+response.status);
  const trace=validateTrace(await response.json());
  const ink=buildInk(trace,rig.area),drives=buildDrives(rig.model,trace),transport=buildTransport(trace);
  const marker=new THREE.Mesh(new THREE.SphereGeometry(.0007,12,8),new THREE.MeshBasicMaterial({color:0xbd292b}));rig.area.add(marker);
  runtime={trace,ink,drives,transport,marker,area:rig.area};
  evidence.state='ready';evidence.traceLoaded=true;evidence.duration=trace.duration;evidence.mapping=ink.mapping;
  evidence.assumedCalibration=trace.settings;$('notice').hidden=true;
  $('status').textContent=new URL(evidence.traceURL).pathname.split('/').pop()+' · recorded physical motion';
  $('calibration').textContent=trace.settings.steps_per_mm.join(' / ')+' steps/mm · '+trace.settings.knob_mm_turn+' mm/knob turn';
  publish();
}
function drawPhysical(time) {
  const r=runtime,p=sampleTrace(r.trace,time),drive=r.drives.set(p);r.ink.setTime(time);
  r.marker.position.set(p.x*r.ink.mapping.factor[0]/1000,-p.y*r.ink.mapping.factor[1]/1000,.00035);
  $('cursor').textContent=p.x.toFixed(2)+' / '+p.y.toFixed(2);
  $('motor').textContent=p.mx.toFixed(2)+' / '+p.my.toFixed(2);
  $('motion-state').textContent=p.erase?'Shaking':p.backlash?'Taking up backlash':time>=r.trace.duration?'Drawing complete':'Line '+p.line;
  Object.assign(evidence,{physical_time:time,playing:r.transport.state.playing,cursor_mm:[p.x,p.y],motor_mm:[p.mx,p.my],
    segment:p.segment,line:p.line,frame:p.frame,backlash:p.backlash,...drive});
}
function animate(now) {
  requestAnimationFrame(animate);
  if(runtime)drawPhysical(runtime.transport.tick(now));
  evidence.camera=optics.optics();optics.composer.render();publish();
}
loadRig().then(loadDrawing).catch(fail);requestAnimationFrame(animate);
