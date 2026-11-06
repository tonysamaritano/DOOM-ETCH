import * as THREE from 'three';
import {frameRGBA} from './decoder.js';
export function buildScreen(screen,area) {
  const ink=new THREE.DataTexture(new Uint8Array(768*576*4),768,576,THREE.RGBAFormat);
  const indexed=new THREE.DataTexture(new Uint8Array(320*200*4),320,200,THREE.RGBAFormat);
  for(const texture of [ink,indexed]) {
    texture.flipY=false;texture.generateMipmaps=false;texture.unpackAlignment=1;
    texture.minFilter=texture.magFilter=THREE.NearestFilter;
    texture.colorSpace=THREE.NoColorSpace;
  }
  ink.minFilter=ink.magFilter=THREE.LinearFilter;
  const material=new THREE.ShaderMaterial({toneMapped:false,
    uniforms:{ink:{value:ink},
      toArea:{value:area.matrixWorld.clone().invert()},extent:{value:new THREE.Vector2(area.userData.width_mm,area.userData.height_mm)}},
    vertexShader:`uniform mat4 toArea;uniform vec2 extent;varying vec2 uvScreen;
      void main(){vec4 world=modelMatrix*vec4(position,1.);uvScreen=(toArea*world).xy*vec2(1000.,-1000.)/extent;
      gl_Position=projectionMatrix*viewMatrix*world;}`,
    fragmentShader:`uniform sampler2D ink;varying vec2 uvScreen;
      void main(){vec2 uv=clamp(uvScreen,0.,1.);
      gl_FragColor=vec4(texture2D(ink,uv).rgb,1.);}`});
  screen.traverse(node=>{if(node.isMesh)node.material=material;});
  return {material,set(frame){
    const texture=frame.metadata.kind==='bgra'?ink:indexed;
    texture.image.data=frameRGBA(frame);texture.needsUpdate=true;material.uniforms.ink.value=texture;
  }};
}
