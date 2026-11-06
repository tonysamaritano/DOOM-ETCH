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
