import * as THREE from 'three';
import {pitchLoop,projectLoop} from './belt-loop.js';
function capturePart(mesh,knob,motor,radiusMM) {
  mesh.geometry=mesh.geometry.clone();mesh.updateMatrixWorld(true);
  const inverse=mesh.matrixWorld.clone().invert(),unit=mesh.getWorldScale(new THREE.Vector3()).x;
  const center=knob.getWorldPosition(new THREE.Vector3()).applyMatrix4(inverse);
  const second=motor.getWorldPosition(new THREE.Vector3()).applyMatrix4(inverse);
  const axis=new THREE.Vector3(0,0,1).applyQuaternion(knob.getWorldQuaternion(new THREE.Quaternion()))
    .applyQuaternion(mesh.getWorldQuaternion(new THREE.Quaternion()).invert()).normalize();
  second.addScaledVector(axis,-second.clone().sub(center).dot(axis));
  const along=second.clone().sub(center).normalize(),across=new THREE.Vector3().crossVectors(axis,along);
  const small=motor.userData.motor_teeth*motor.userData.belt_pitch_mm/(2*Math.PI)/1000/unit;
  const loop=pitchLoop(radiusMM/1000/unit,small,second.distanceTo(center));
  const position=mesh.geometry.attributes.position,normal=mesh.geometry.attributes.normal,vertices=[];
  for(let i=0;i<position.count;i++) {
    const v=new THREE.Vector3().fromBufferAttribute(position,i).sub(center);
    const hit=projectLoop(loop,v.dot(along),v.dot(across)),p=loop.point(hit.u);
    const outward=along.clone().multiplyScalar(p[2]).addScaledVector(across,p[3]);
    const tangent=new THREE.Vector3().crossVectors(axis,outward);
    const n=normal?new THREE.Vector3().fromBufferAttribute(normal,i):new THREE.Vector3();
    vertices.push([hit.u,hit.offset,v.dot(axis),n.dot(outward),n.dot(tangent),n.dot(axis)]);
  }
  return {mesh,position,normal,vertices,center,axis,along,across,loop,unit};
}
