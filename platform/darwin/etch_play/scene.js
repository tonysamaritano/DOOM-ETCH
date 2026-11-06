import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {buildStore} from './shared/store.js';
import {buildDrives} from './shared/drives.js';
import {buildScreen} from './screen.js';
export async function buildScene(renderer,read) {
  const scene=new THREE.Scene();buildStore(scene,renderer);
  const manager=new THREE.LoadingManager();
  manager.setURLModifier(url=>{if(!url.startsWith('blob:')&&!url.startsWith('data:'))throw Error('External GLB resources are not allowed');return url;});
  const gltf=await new GLTFLoader(manager).parseAsync(await read('rig.glb'),'');
  const model=gltf.scene;scene.add(model);model.updateMatrixWorld(true);
  const area=model.getObjectByName('DrawArea'),screen=model.getObjectByName('Screen');
  for(const name of ['Screen','DrawArea','KnobX','KnobY','MotorX','MotorY'])
    if(!model.getObjectByName(name))throw Error('Missing rig node '+name);
  if(area.parent!==screen||!(area.userData.width_mm>0&&area.userData.height_mm>0))throw Error('Invalid screen opening');
  const display=buildScreen(screen,area);
  const noteURL=new URL('bezel-note.png',location.href).href;
  const paper=await new THREE.TextureLoader().loadAsync(noteURL);paper.colorSpace=THREE.SRGBColorSpace;
  const note=new THREE.Mesh(new THREE.PlaneGeometry(.0375,.0373),
    new THREE.MeshBasicMaterial({map:paper,transparent:true,depthTest:false,depthWrite:false,toneMapped:false}));
  note.name='Cheat note';note.position.set(area.userData.width_mm/1000+.021,-.049,.004);note.renderOrder=20;area.add(note);
  model.traverse(node=>{if(node.isMesh){node.castShadow=true;node.receiveShadow=true;}});model.updateMatrixWorld(true);
  const camera=new THREE.PerspectiveCamera(32,1,.005,20);camera.up.set(0,0,1);
  const bounds=rigBounds(model),center=area.localToWorld(new THREE.Vector3(area.userData.width_mm/2000,-area.userData.height_mm/2000,0));
  const up=new THREE.Vector3(0,1,0).transformDirection(area.matrixWorld);
  const normal=new THREE.Vector3(0,0,1).transformDirection(area.matrixWorld);
  const side=new THREE.Vector3(1,0,0).transformDirection(area.matrixWorld);
  const target=center.clone().addScaledVector(up,-.022),back=normal.clone().addScaledVector(side,.09).add(new THREE.Vector3(0,0,.10)).normalize();
  return createSceneState({renderer,scene,model,area,display,noteURL,camera,bounds,target,back,nodes:gltf.parser.json.nodes.length});
}
function rigBounds(model) {
  const bounds=new THREE.Box3();model.traverse(node=>{
    if(!node.isMesh)return;
    for(let parent=node;parent;parent=parent.parent)if(/tabletop/i.test(parent.name))return;
    node.geometry.computeBoundingBox();bounds.union(node.geometry.boundingBox.clone().applyMatrix4(node.matrixWorld));
  });return bounds;
}
function frameCamera(camera,bounds,target,back,width,height) {
  camera.aspect=width/height;camera.updateProjectionMatrix();
  const right=new THREE.Vector3().crossVectors(camera.up,back).normalize();
  const up=new THREE.Vector3().crossVectors(back,right).normalize();
  const tan=Math.tan(THREE.MathUtils.degToRad(camera.fov/2));let distance=.25;
  for(const x of [bounds.min.x,bounds.max.x])for(const y of [bounds.min.y,bounds.max.y])for(const z of [bounds.min.z,bounds.max.z]) {
    const point=new THREE.Vector3(x,y,z).sub(target),depth=point.dot(back);
    distance=Math.max(distance,depth+Math.abs(point.dot(right))*1.06/(tan*camera.aspect),depth+Math.abs(point.dot(up))*1.06/tan);
  }
  camera.position.copy(target).addScaledVector(back,distance);camera.lookAt(target);
}
function createSceneState(context) {
  const c=context;let drives=null;
  const calibration={settings:{knob_mm_turn:20}};
  return {scene:c.scene,camera:c.camera,
    resize(width,height){if(width<1||height<1)return;frameCamera(c.camera,c.bounds,c.target,c.back,width,height);c.renderer.setSize(width,height,false);},
    apply(frame){
      c.display.set(frame);const m=frame.metadata;let motion=null;
      if(m.mode==='physical') {
        if(!drives)drives=buildDrives(c.model,calibration);
        calibration.settings.knob_mm_turn=m.knob_mm_turn;
        motion=drives.set({mx:m.motor_mm[0],my:m.motor_mm[1]});
      }
      return motion;
    },render(){c.renderer.render(c.scene,c.camera);},
    evidence(){return {nodes:c.nodes,noteURL:c.noteURL,
      visible_opening_mm:[c.area.userData.width_mm,c.area.userData.height_mm],
      camera:{position:c.camera.position.toArray(),target:c.target.toArray(),aspect:c.camera.aspect}};}
  };
}
