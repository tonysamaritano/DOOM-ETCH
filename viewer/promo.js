// SPDX-License-Identifier: GPL-2.0-or-later
export const PITCH_DURATION=18;
const starts=[0,3,6,9,12.5,15],names=['bezel','rig','screen','drive','speed','price'];
const clamp=x=>Math.min(1,Math.max(0,x));
const ease=x=>{x=clamp(x);return x*x*(3-2*x);};
const span=(t,a,b)=>clamp((t-a)/(b-a));
export function pitchContext(trace) {
  const records=trace.requirements;
  const metric=records?.find(r=>r.frame===0&&r.target_fps===30);
  if(!Number.isFinite(trace.duration)||trace.duration<=3.5||trace.erases.length||
    !trace.segments.length||trace.segments.length%15||records?.length!==1||
    !metric||metric.geometry!=='commanded-step-grid'||metric.budget!=='drawing-only-one-frame')
    throw Error('Pitch needs a completed single-frame trace with frame 0 requirements at 30 fps');
  for(const key of ['path_m','no_stop_m_s','no_stop_mach','accel_g','peak_mach','knob_rpm'])
    if(!Number.isFinite(metric[key])||metric[key]<=0)throw Error('Invalid pitch metric: '+key);
  let interval=null;
  for(let i=0;i<trace.segments.length;i+=15) {
    const a=trace.segments.slice(i,i+15);
    if(a[14]!==0)throw Error('Pitch accepts frame 0 only');
    if(!interval&&a[0]>=trace.duration*.75&&a[0]+3.5<=trace.duration&&a[1]>0&&a[6]>0)
      interval={start:a[0],end:a[0]+3.5,line:a[13],segment:i/15};
  }
  if(!interval)throw Error('Pitch trace needs a late 3.5-second drawing interval');
  return {duration:trace.duration,interval,metric:{...metric},minutes:trace.duration/60};
}
export function buildPitchState(context,t) {
  if(!Number.isFinite(t)||t<0||t>PITCH_DURATION)throw new RangeError('Pitch time must be within 0..18 seconds');
  let shot=starts.findLastIndex(start=>start<=t),local=t-starts[shot];
  const duration=(starts[shot+1]??18)-starts[shot];
  return {cut:'pitch',video_time:t,shot:names[shot],shot_index:shot,local,
    progress:ease(local/duration),physical_time:shot===3?context.interval.start+local:context.duration,
    interval:{...context.interval},metric:{...context.metric},minutes:context.minutes,
    price:{text:'1 SUBSCRIBE!',tilt:-.06,pop:ease(span(local,.1,.35)),
      pulse:1+.035*Math.sin(local*Math.PI*2.2)},
    commitment:'10,000 subscribers and I build it.',commitment_alpha:shot===5?span(local,1,1.4):0,
    fine_alpha:shot===5?span(local,.6,.8):0};
}

const dealFont=size=>`italic 900 ${size}px "Arial Black",Impact,"Helvetica Neue",sans-serif`;
function measured(text,size,measure,font=dealFont) {
  const metrics=measure(text,font(size));
  if(!Number.isFinite(metrics.width)||metrics.width<=0)throw Error('Cannot measure pitch text');
  const ascent=metrics.actualBoundingBoxAscent??size*.8,descent=metrics.actualBoundingBoxDescent??size*.2;
  const left=metrics.actualBoundingBoxLeft??metrics.width/2,right=metrics.actualBoundingBoxRight??metrics.width/2;
  return {text,font:font(size),width:Math.max(metrics.width,left+right),height:ascent+descent,
    textOffset:[(left-right)/2,(ascent-descent)/2]};
}
function bounds(cx,cy,rx,ry,angle) {
  const x=Math.abs(rx*Math.cos(angle))+Math.abs(ry*Math.sin(angle));
  const y=Math.abs(rx*Math.sin(angle))+Math.abs(ry*Math.cos(angle));
  return {left:cx-x,right:cx+x,top:cy-y,bottom:cy+y};
}
export function layoutPitch(state,width,height,measure) {
  if(!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)
    throw new RangeError('Invalid pitch viewport');
  let size=Math.min(height*.085,width*.11),price=measured(state.price.text,size,measure);
  size*=Math.min(1,width*.39/price.width,height*.1/price.height);
  price=measured(state.price.text,size,measure);
  const cx=width*.5,cy=height*.22,rx=price.width*.85/.78,ry=price.height*1.35/.78;
  const scale=state.price.pop,pulse=state.price.pulse;
  const polygon=Array.from({length:44},(_,i)=>{
    const angle=i*Math.PI/22,inner=i%2?.78:1;
    return [Math.cos(angle)*rx*inner*pulse,Math.sin(angle)*ry*inner*pulse];
  });
  const textBounds=bounds(cx,cy,price.width*.5*scale,price.height*.5*scale,state.price.tilt);
  return {width,height,price:{...price,center:[cx,cy],tilt:state.price.tilt,scale,pulse,
    rx,ry,polygon,textBounds,burstBounds:bounds(cx,cy,rx*pulse*scale,ry*pulse*scale,state.price.tilt)},
    commitment:{text:state.commitment,center:[cx,height*.855],alpha:state.commitment_alpha},
    fine:{text:'Device not built yet. Assumed calibration. Simulated results. More screens? Faster drawing? Let’s find out.',
      top:height*.925,height:height*.075,alpha:state.fine_alpha}};
}
