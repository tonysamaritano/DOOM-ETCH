import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {buildStore} from './store.js';
import {buildCamera,rigBounds} from './camera.js';
import {createPitch,drawPitchOverlay,pitchOptions} from './promo.js';
import {startCapture} from './capture.js';
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
  const url=new URLSearchParams(location.search).get('trace') ?? '../build/viewer/e1m1.json';
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

function pitchAnchors(rig) {
  rig.model.updateMatrixWorld(true);
  const box=rigBounds(rig.model),corners=[];
  for(const x of [box.min.x,box.max.x])for(const y of [box.min.y,box.max.y])
    for(const z of [box.min.z,box.max.z])corners.push(new THREE.Vector3(x,y,z));
  const w=rig.area.userData.width_mm/1000,h=rig.area.userData.height_mm/1000;
  const screen=[];
  for(const x of [0,w])for(const y of [0,-h])screen.push(rig.area.localToWorld(new THREE.Vector3(x,y,0)));
  const center=rig.area.localToWorld(new THREE.Vector3(w/2,-h/2,0));
  const drive=rig.model.getObjectByName('MotorX').getWorldPosition(new THREE.Vector3())
    .lerp(rig.model.getObjectByName('KnobX').getWorldPosition(new THREE.Vector3()),.35);
  return {corners,screen,center,drive,rig:box.getCenter(new THREE.Vector3()),
    normal:new THREE.Vector3(0,0,1).transformDirection(rig.area.matrixWorld),
    up:new THREE.Vector3(0,1,0).transformDirection(rig.area.matrixWorld),
    side:new THREE.Vector3(1,0,0).transformDirection(rig.area.matrixWorld)};
}
function pitchCamera(state,a,width,height) {
  const p=state.progress,detail=['bezel','screen','drive'].includes(state.shot);
  let target=detail?a.center.clone():a.rig.clone(),points=detail?a.screen:a.corners;
  let back=a.normal.clone().addScaledVector(a.up,-.7).normalize(),lens=45;
  if(state.shot==='bezel'){target.addScaledVector(a.up,.02);back.addScaledVector(a.side,.2-.4*p).normalize();lens=65;}
  if(state.shot==='screen'){target.addScaledVector(a.up,-.012*p);lens=60;}
  if(state.shot==='drive'){target.lerp(a.drive,.35);points=[...a.screen,a.drive];back.addScaledVector(a.side,-.2+.4*p).normalize();lens=65;}
  if(!detail)back=new THREE.Vector3(.34,-.48,.25).normalize()
    .applyAxisAngle(new THREE.Vector3(0,0,1),state.shot==='price'?.05:-.3+.5*p);
  const projection=new THREE.PerspectiveCamera(35,width/height);projection.setFocalLength(lens);
  const right=new THREE.Vector3().crossVectors(new THREE.Vector3(0,0,1),back).normalize();
  const up=new THREE.Vector3().crossVectors(back,right).normalize();
  const tan=Math.tan(THREE.MathUtils.degToRad(projection.fov/2));let distance=.25;
  for(const point of points) {
    const v=point.clone().sub(target),depth=v.dot(back);
    distance=Math.max(distance,depth+1.16*Math.abs(v.dot(right))/(tan*projection.aspect),
      depth+1.16*Math.abs(v.dot(up))/tan);
  }
  if(state.shot==='price')distance*=1.1-.04*p;
  return {position:target.clone().addScaledVector(back,distance).toArray(),target:target.toArray(),
    lens_mm:lens,aperture_f:5.6,focus_m:distance};
}
function buildPitch(rig) {
  const anchors=pitchAnchors(rig),canvas=document.createElement('canvas');
  canvas.id='promo-canvas';canvas.setAttribute('aria-label','Eighteen-second machine pitch');stage.append(canvas);
  const ctx=canvas.getContext('2d');if(!ctx)throw Error('Pitch needs a 2D composition canvas');
  let fixed=null,last=0;
  const pitch=createPitch(runtime.trace,{
    size:()=>fixed??[Math.max(1,stage.clientWidth),Math.max(1,stage.clientHeight)],
    measure(text,font){ctx.font=font;ctx.textAlign='center';ctx.textBaseline='alphabetic';return ctx.measureText(text);},
    paint(state,layout) {
      const {width,height}=layout;
      if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;}
      drawPhysical(state.physical_time);
      const camera=optics.script(pitchCamera(state,anchors,width,height),width,height);
      optics.composer.render();ctx.drawImage(renderer.domElement,0,0,width,height);
      drawPitchOverlay(ctx,state,layout);last=state.video_time;
      evidence.camera=camera;evidence.promo={...state,layout,render_size:[width,height]};publish();return camera;
    }
  });
  return {canvas,renderAt:pitch.renderAt,setSize(width,height) {
    fixed=[width,height];pitch.renderAt(last);
  }};
}
