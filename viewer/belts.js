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
function movePart(part,angle) {
  const b=part,travel=b.loop.big*angle;
  for(let i=0;i<b.vertices.length;i++) {
    const [u,offset,depth,no,nt,nw]=b.vertices[i],p=b.loop.point(u+travel);
    const outward=b.along.clone().multiplyScalar(p[2]).addScaledVector(b.across,p[3]);
    const tangent=new THREE.Vector3().crossVectors(b.axis,outward);
    const v=b.center.clone().addScaledVector(b.along,p[0]).addScaledVector(b.across,p[1])
      .addScaledVector(outward,offset).addScaledVector(b.axis,depth);
    b.position.setXYZ(i,v.x,v.y,v.z);
    if(b.normal){const n=outward.multiplyScalar(no).addScaledVector(tangent,nt).addScaledVector(b.axis,nw);b.normal.setXYZ(i,n.x,n.y,n.z);}
  }
  b.position.needsUpdate=true;if(b.normal)b.normal.needsUpdate=true;
}
export function buildBelts(model) {
  const belts=[];model.updateMatrixWorld(true);
  model.traverse(node=>{
    const axis=node.name[0]?.toLowerCase();if(!node.userData['Pitch length mm']||!['x','y'].includes(axis))return;
    const knob=model.getObjectByName('Knob'+axis.toUpperCase()),motor=model.getObjectByName('Motor'+axis.toUpperCase());
    let pulley;model.traverse(o=>{if(o.name[0]?.toLowerCase()===axis&&o.userData['Pitch diameter mm'])pulley=o;});
    if(!knob||!motor||!pulley)throw Error('Missing belt drive contract');
    const radiusMM=pulley.userData['Pitch diameter mm']/2,parts=[];
    node.traverse(mesh=>{if(mesh.isMesh)parts.push(capturePart(mesh,knob,motor,radiusMM));});
    belts.push({axis,parts,radiusMM,phaseMM:0});
  });
  return {set(angles){for(const b of belts){for(const part of b.parts)movePart(part,angles[b.axis]);b.phaseMM=b.radiusMM*angles[b.axis];}},
    evidence(){return belts.map(b=>({axis:b.axis,travel_mm:b.phaseMM,loop_mm:b.parts[0].loop.length*b.parts[0].unit*1000,phase_mm:((b.phaseMM%220)+220)%220}));}};
}
