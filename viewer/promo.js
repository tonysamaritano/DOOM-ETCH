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
function fitText(text,size,maxWidth,measure,font=dealFont) {
  let value=measured(text,size,measure,font);
  if(value.width>maxWidth)value=measured(text,size*maxWidth/value.width,measure,font);
  return value;
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
  const commitment=fitText(state.commitment,height*.043,width*.9,measure);
  const fine=fitText('Device not built yet. Assumed calibration. Simulated results. More screens? Faster drawing? Let’s find out.',
    height*.018,width*.94,measure,size=>`500 ${size}px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif`);
  commitment.center=[cx,height*.855];fine.center=[cx,height*.9625];
  for(const value of [commitment,fine])value.bounds=bounds(...value.center,value.width/2,value.height/2,0);
  return {width,height,price:{...price,center:[cx,cy],tilt:state.price.tilt,scale,pulse,
    rx,ry,polygon,textBounds,burstBounds:bounds(cx,cy,rx*pulse*scale,ry*pulse*scale,state.price.tilt)},
    commitment:{...commitment,alpha:state.commitment_alpha},
    fine:{...fine,top:height*.925,height:height*.075,alpha:state.fine_alpha}};
}

function drawPrice(ctx,price) {
  if(price.scale<=0)return;
  ctx.save();ctx.translate(...price.center);ctx.rotate(price.tilt);ctx.scale(price.scale,price.scale);
  ctx.beginPath();price.polygon.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();
  ctx.fillStyle='#ffe65c';ctx.strokeStyle='#292421';ctx.lineWidth=3;ctx.fill();ctx.stroke();
  ctx.font=price.font;ctx.textAlign='center';ctx.textBaseline='alphabetic';
  ctx.fillStyle='#bd292b';ctx.strokeStyle='#fff9df';ctx.lineWidth=3;
  ctx.strokeText(price.text,...price.textOffset);ctx.fillText(price.text,...price.textOffset);ctx.restore();
}
function caption(ctx,text,x,y,size,maxWidth,color='#fff9df') {
  ctx.save();ctx.font=dealFont(size);
  const width=ctx.measureText(text).width;
  if(width>maxWidth)ctx.font=dealFont(size*maxWidth/width);
  ctx.textAlign='center';ctx.textBaseline='middle';ctx.lineJoin='round';
  ctx.strokeStyle='#352823';ctx.lineWidth=Math.max(2,size*.065);ctx.fillStyle=color;
  ctx.strokeText(text,x,y);ctx.fillText(text,x,y);ctx.restore();
}
function drawMeasured(ctx,value) {
  ctx.save();ctx.translate(...value.center);ctx.font=value.font;
  ctx.textAlign='center';ctx.textBaseline='alphabetic';ctx.lineJoin='round';
  ctx.strokeStyle='#352823';ctx.lineWidth=2;ctx.fillStyle='#fff9df';
  ctx.strokeText(value.text,...value.textOffset);ctx.fillText(value.text,...value.textOffset);ctx.restore();
}
export function drawPitchOverlay(ctx,state,layout) {
  const {width:w,height:h}=layout;ctx.clearRect(0,0,w,h);
  if(state.shot==='drive'||state.shot==='speed') {
    const speed=state.shot==='speed';
    caption(ctx,speed?'TOP SPEED REQUIRED:':'REFRESH RATE:',w*.5,h*.19,h*.05,w*.9);
    caption(ctx,speed?`MACH ${state.metric.no_stop_mach.toFixed(2)}`:
      `1 FRAME / ${state.minutes.toFixed(1)} MIN`,w*.5,h*.29,h*.085,w*.9,'#ffe65c');
    caption(ctx,speed?'No stops · 30 frames per second':'Actual simulated physical time',w*.5,h*.37,h*.023,w*.9);
  }
  if(state.shot!=='price')return;
  drawPrice(ctx,layout.price);
  ctx.save();ctx.globalAlpha=layout.commitment.alpha;
  drawMeasured(ctx,layout.commitment);ctx.restore();
  const fine=layout.fine;ctx.save();ctx.globalAlpha=fine.alpha;ctx.fillStyle='#292421';
  ctx.fillRect(0,fine.top,w,fine.height);
  drawMeasured(ctx,fine);ctx.restore();
}

export function createPitch(trace,adapter) {
  const context=pitchContext(trace);
  return {context,renderAt(t) {
    const state=buildPitchState(context,t),[width,height]=adapter.size();
    const layout=layoutPitch(state,width,height,adapter.measure);
    const camera=adapter.paint(state,layout);
    return {...state,layout,camera};
  }};
}
