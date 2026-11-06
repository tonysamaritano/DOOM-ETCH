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
