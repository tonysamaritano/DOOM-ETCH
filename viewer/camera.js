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
