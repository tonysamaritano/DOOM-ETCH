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
