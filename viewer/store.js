import * as THREE from 'three';
import {RoomEnvironment} from 'three/addons/environments/RoomEnvironment.js';
function box(group,size,position,material) {
  const mesh=new THREE.Mesh(new THREE.BoxGeometry(...size),material);
  mesh.position.set(...position);mesh.receiveShadow=true;group.add(mesh);return mesh;
}
export function buildStore(scene,renderer) {
  scene.background=new THREE.Color(0xf1f2ee);
  const room=new RoomEnvironment(),pmrem=new THREE.PMREMGenerator(renderer);
  scene.environment=pmrem.fromScene(room,.04).texture;room.dispose();pmrem.dispose();
  const group=new THREE.Group();group.name='Bright shop';scene.add(group);
  const plaster=new THREE.MeshStandardMaterial({color:0xf4f4ed,roughness:.9});
  const limestone=new THREE.MeshStandardMaterial({color:0xd9dbd2,roughness:.85});
  const oak=new THREE.MeshStandardMaterial({color:0xc6a17b,roughness:.65});
  box(group,[8,8,.05],[0,0,-.88],limestone);
  box(group,[8,.05,3],[0,3,.6],plaster);box(group,[.05,8,3],[-3,0,.6],plaster);
  for(const x of [-.29,.29])for(const y of [-.23,.23])box(group,[.035,.035,.8],[x,y,-.45],oak);
  for(const x of [-1.5,1.5]) {
    box(group,[.9,.65,.035],[x,1,-.05],oak);
    for(const dx of [-.36,.36])for(const y of [.76,1.24])box(group,[.035,.035,.8],[x+dx,y,-.46],oak);
  }
  const fill=new THREE.HemisphereLight(0xffffff,0xa5a79b,2);fill.position.set(0,0,3);scene.add(fill);
  const key=new THREE.DirectionalLight(0xfff7e9,3.5);key.position.set(-1,-1,3);key.castShadow=true;
  key.shadow.mapSize.set(1024,1024);key.shadow.camera.left=-1;key.shadow.camera.right=1;
  key.shadow.camera.top=1;key.shadow.camera.bottom=-1;key.shadow.normalBias=.002;scene.add(key);
  const daylight=new THREE.DirectionalLight(0xeaf3ff,1.6);daylight.position.set(2,-1,2);scene.add(daylight);
  return group;
}
