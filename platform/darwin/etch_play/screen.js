import * as THREE from 'three';
export function buildScreen(screen,area) {
  const indices=new THREE.DataTexture(new Uint8Array(64000),320,200,THREE.RedFormat);
  const palette=new THREE.DataTexture(new Uint8Array(1024),256,1,THREE.RGBAFormat);
  const ink=new THREE.DataTexture(new Uint8Array(768*576*4),768,576,THREE.RGBAFormat);
  for(const texture of [indices,palette,ink]) {
    texture.flipY=false;texture.generateMipmaps=false;texture.unpackAlignment=1;
    texture.minFilter=texture.magFilter=THREE.NearestFilter;
    texture.colorSpace=THREE.NoColorSpace;
  }
  const material=new THREE.ShaderMaterial({toneMapped:false,
    uniforms:{indices:{value:indices},palette:{value:palette},ink:{value:ink},bgra:{value:false},
      toArea:{value:area.matrixWorld.clone().invert()},extent:{value:new THREE.Vector2(area.userData.width_mm,area.userData.height_mm)}},
    vertexShader:`uniform mat4 toArea;uniform vec2 extent;varying vec2 uvScreen;
      void main(){vec4 world=modelMatrix*vec4(position,1.);uvScreen=(toArea*world).xy*vec2(1000.,-1000.)/extent;
      gl_Position=projectionMatrix*viewMatrix*world;}`,
    fragmentShader:`uniform sampler2D indices,palette,ink;uniform bool bgra;varying vec2 uvScreen;
      void main(){vec2 uv=clamp(uvScreen,0.,1.);
      if(bgra){gl_FragColor=vec4(texture2D(ink,uv).bgr,1.);return;}
      float i=texture2D(indices,uv).r;gl_FragColor=texture2D(palette,vec2((i*255.+.5)/256.,.5));}`});
  screen.traverse(node=>{if(node.isMesh)node.material=material;});
  return {material,set(frame){
    material.uniforms.bgra.value=frame.metadata.kind==='bgra';
    if(frame.metadata.kind==='bgra'){ink.image.data=frame.pixels;ink.needsUpdate=true;}
    else{indices.image.data=frame.pixels;palette.image.data=frame.palette;indices.needsUpdate=palette.needsUpdate=true;}
  }};
}
