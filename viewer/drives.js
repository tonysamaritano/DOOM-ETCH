import * as THREE from 'three';
import {buildBelts} from './belts.js';
export function buildDrives(model,trace) {
  const drives=[];
  for(const axis of ['x','y'])for(const kind of ['Knob','Motor']) {
    const node=model.getObjectByName(kind+axis.toUpperCase());
    if(!node)throw Error('Missing '+kind+axis.toUpperCase());
    const ratio=kind==='Motor'?node.userData.etch_ratio:1;
    if(!(ratio>0))throw Error('Invalid motor ratio');
    drives.push({node,axis,ratio,rest:node.quaternion.clone(),kind});
  }
  const belts=buildBelts(model),spin=new THREE.Vector3(0,0,1);
  return {set(point){
    const angles={x:-2*Math.PI*point.mx/trace.settings.knob_mm_turn,
      y:2*Math.PI*point.my/trace.settings.knob_mm_turn};
    for(const d of drives)d.node.quaternion.copy(d.rest).multiply(new THREE.Quaternion().setFromAxisAngle(spin,angles[d.axis]*d.ratio));
    belts.set(angles);
    return {knob_radians:[angles.x,angles.y],motor_radians:drives.filter(d=>d.kind==='Motor').map(d=>angles[d.axis]*d.ratio),belts:belts.evidence()};
  }};
}
