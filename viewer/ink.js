import * as THREE from 'three';
const vertex=`
attribute vec4 timing,profile,path;
uniform float physicalTime,shakes;
varying vec2 capsule,mm;varying float drawn,alpha;
void main(){
 float t=clamp(physicalTime-timing.x,0.,timing.y),distance;
 if(t>=timing.y)distance=profile.z+profile.y;
 else if(t<timing.z)distance=.5*profile.x*t*t;
 else if(timing.y-t<timing.z){float left=timing.y-t;distance=profile.z+profile.y-.5*profile.x*left*left;}
 else distance=timing.w*(t-.5*timing.z);
 drawn=t>=timing.y?profile.z:clamp(distance-profile.y,0.,profile.z);
 if(physicalTime<timing.x||drawn<=0.){gl_Position=vec4(2.,2.,2.,1.);return;}
 float radius=.18;capsule=vec2(mix(-radius,drawn+radius,position.x),position.y*radius);
 mm=path.xy+path.zw*capsule.x+vec2(-path.w,path.z)*capsule.y;
 alpha=pow(.08,max(0.,shakes-profile.w));
 gl_Position=projectionMatrix*modelViewMatrix*vec4(mm.x,-mm.y,0.,1.);
}`;
const fragment=`
uniform vec2 travel;varying vec2 capsule,mm;varying float drawn,alpha;
void main(){
 if(mm.x<0.||mm.y<0.||mm.x>travel.x||mm.y>travel.y)discard;
 float distance=length(vec2(capsule.x-clamp(capsule.x,0.,drawn),capsule.y));
 float aa=max(fwidth(distance),.015),coverage=1.-smoothstep(.065-aa,.065+aa,distance);
 if(coverage*alpha<.002)discard;
 gl_FragColor=vec4(.12,.14,.13,coverage*alpha);
}`;
export function buildInk(trace,area) {
  const geometry=new THREE.InstancedBufferGeometry();
  geometry.setAttribute('position',new THREE.Float32BufferAttribute([0,-1,0,1,-1,0,0,1,0,1,1,0],3));
  geometry.setIndex([0,1,2,2,1,3]);
  const count=trace.segments.length/15,timing=[],profile=[],path=[];
  for(let i=0;i<count;i++) {
    const a=trace.segments.slice(i*15,i*15+15);
    timing.push(...a.slice(0,4));profile.push(a[4],a[5],a[6],a[14]);path.push(...a.slice(7,11));
  }
  for(const [name,values] of Object.entries({timing,profile,path}))
    geometry.setAttribute(name,new THREE.InstancedBufferAttribute(Float32Array.from(values),4));
  geometry.instanceCount=count;
  const uniforms={physicalTime:{value:0},shakes:{value:0},
    travel:{value:new THREE.Vector2(trace.settings.width_mm,trace.settings.height_mm)}};
  const material=new THREE.ShaderMaterial({uniforms,vertexShader:vertex,fragmentShader:fragment,
    transparent:true,depthWrite:false,side:THREE.DoubleSide});
  const mesh=new THREE.Mesh(geometry,material);mesh.frustumCulled=false;mesh.renderOrder=2;
  const sx=area.userData.width_mm/trace.settings.width_mm/1000;
  const sy=area.userData.height_mm/trace.settings.height_mm/1000;
  mesh.scale.set(sx,sy,.001);mesh.position.z=.0001;area.add(mesh);
  return {mesh,setTime(time){uniforms.physicalTime.value=time;uniforms.shakes.value=trace.erases.reduce((n,[start,duration])=>n+Math.max(0,Math.min(1,(time-start)/duration)),0);},
    mapping:{physical:[trace.settings.width_mm,trace.settings.height_mm],visible:[area.userData.width_mm,area.userData.height_mm],factor:[sx*1000,sy*1000]}};
}
