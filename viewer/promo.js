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
