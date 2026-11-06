import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {EffectComposer} from 'three/addons/postprocessing/EffectComposer.js';
import {RenderPass} from 'three/addons/postprocessing/RenderPass.js';
import {BokehPass} from 'three/addons/postprocessing/BokehPass.js';
import {OutputPass} from 'three/addons/postprocessing/OutputPass.js';
export function rigBounds(model) {
  const bounds=new THREE.Box3();model.updateMatrixWorld(true);
  model.traverse(node=>{
    if(!node.isMesh)return;
    for(let p=node;p;p=p.parent)if(/tabletop/i.test(p.name))return;
    node.geometry.computeBoundingBox();bounds.union(node.geometry.boundingBox.clone().applyMatrix4(node.matrixWorld));
  });return bounds;
}
function fit(camera,controls,bounds) {
  const target=bounds.getCenter(new THREE.Vector3()),back=new THREE.Vector3(.34,-.48,.25).normalize();
  const right=new THREE.Vector3().crossVectors(camera.up,back).normalize();
  const up=new THREE.Vector3().crossVectors(back,right).normalize();
  const tan=Math.tan(THREE.MathUtils.degToRad(camera.fov/2));let distance=.35;
  for(const x of [bounds.min.x,bounds.max.x])for(const y of [bounds.min.y,bounds.max.y])for(const z of [bounds.min.z,bounds.max.z]) {
    const p=new THREE.Vector3(x,y,z).sub(target),depth=p.dot(back);
    distance=Math.max(distance,depth+1.12*Math.abs(p.dot(right))/(tan*camera.aspect),depth+1.12*Math.abs(p.dot(up))/tan);
  }
  camera.position.copy(target).addScaledVector(back,distance);controls.target.copy(target);controls.update();
}
export function buildCamera(scene,renderer,stage) {
  const camera=new THREE.PerspectiveCamera(35,1,.005,15);camera.up.set(0,0,1);camera.setFocalLength(45);
  const controls=new OrbitControls(camera,renderer.domElement);controls.minDistance=.15;controls.maxDistance=3;
  camera.position.set(.4,-.6,.35);controls.target.set(0,0,.12);controls.update();
  controls.enableDamping=false;let bounds=null,autofocus=true;
  const composer=new EffectComposer(renderer);composer.addPass(new RenderPass(scene,camera));
  const bokeh=new BokehPass(scene,camera,{focus:.65,aperture:.02,maxblur:.012});composer.addPass(bokeh);composer.addPass(new OutputPass());
  const inputs=Object.fromEntries(['lens','aperture','focus'].map(id=>[id,document.getElementById(id)]));
  function optics() {
    camera.setFocalLength(Number(inputs.lens.value));
    const focus=Math.max(.01,autofocus?camera.position.distanceTo(controls.target):Number(inputs.focus.value));
    if(autofocus)inputs.focus.value=focus;
    const focal=Number(inputs.lens.value)/1000,fstop=Number(inputs.aperture.value);
    bokeh.uniforms.focus.value=focus;bokeh.uniforms.aperture.value=focal*focal/(fstop*focus*Math.max(.001,focus-focal)*.036);
    for(const [id,value] of Object.entries({lens:inputs.lens.value+' mm',aperture:'f/'+fstop.toFixed(1),focus:focus.toFixed(2)+' m'}))document.getElementById(id+'-value').textContent=value;
    return {lens_mm:Number(inputs.lens.value),aperture_f:fstop,focus_m:focus,position:camera.position.toArray(),target:controls.target.toArray()};
  }
  for(const [id,input] of Object.entries(inputs))input.addEventListener('input',()=>{if(id==='focus')autofocus=false;optics();});
  document.getElementById('reset-camera').onclick=()=>{inputs.lens.value=45;inputs.aperture.value=5.6;autofocus=true;camera.setFocalLength(45);if(bounds)fit(camera,controls,bounds);optics();};
  function resize(){camera.aspect=stage.clientWidth/stage.clientHeight;camera.updateProjectionMatrix();renderer.setSize(stage.clientWidth,stage.clientHeight,false);composer.setSize(stage.clientWidth,stage.clientHeight);if(bounds)fit(camera,controls,bounds);optics();}
  new ResizeObserver(resize).observe(stage);
  return {camera,controls,composer,optics,setModel(model){bounds=rigBounds(model);resize();}};
}
