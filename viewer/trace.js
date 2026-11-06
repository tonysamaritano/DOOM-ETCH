export const fields = ['t0','duration','ramp','peak','accel','slack','length',
  'x0','y0','ux','uy','motor_x0','motor_y0','line','frame'];
export function validateTrace(data) {
  if(data.format!=='doom-etch-trace'||data.version!==1||
    JSON.stringify(data.segment_fields)!==JSON.stringify(fields))throw Error('Unsupported trace schema');
  const s=data.settings;
  if(!s||![s.width_mm,s.height_mm,s.knob_mm_turn,...(s.steps_per_mm||[])].every(x=>Number.isFinite(x)&&x>0)||
    s.steps_per_mm.length!==2||s.width_mm*s.steps_per_mm[0]>2**52||s.height_mm*s.steps_per_mm[1]>2**52)throw Error('Invalid physical dimensions or calibration');
  if(!Array.isArray(data.start)||data.start.length!==2||!Array.from(data.start).every(Number.isFinite)||
    !Number.isFinite(data.duration)||!Number.isFinite(data.simulated_seconds)||data.duration<0||data.simulated_seconds<0||Math.abs(data.duration-data.simulated_seconds)>1e-9)
    throw Error('Invalid trace start or completed clock');
  if(!Array.isArray(data.segments)||data.segments.length%15||!Array.from(data.segments).every(Number.isFinite))
    throw Error('Invalid segment records');
  if(!Array.isArray(data.erases)||!data.erases.every(e=>Array.isArray(e)&&e.length===2&&e.every(Number.isFinite)&&e[0]>=0&&e[1]>0))
    throw Error('Invalid erase records');
  for(let i=0;i<data.segments.length;i+=15) {
    const a=data.segments.slice(i,i+15);
    if(a[0]<0||a[1]<0||a[2]<0||a[3]<0||a[4]<0||a[5]<0||a[6]<0||a[0]+a[1]>data.duration+1e-9||
      (i&&a[0]<data.segments[i-15]))throw Error('Invalid segment timing');
  }
  return data;
}
export function quantize(value,steps,extent) {
  if(![value,steps,extent].every(Number.isFinite)||steps<=0||extent<=0||
    !Number.isFinite(extent*steps)||extent*steps>2**52)throw new RangeError('Invalid step grid');
  return Math.max(0,Math.min(Math.round(value*steps),Math.floor(extent*steps)))/steps;
}
export function sampleSegment(trace,index,time) {
  if(!Number.isFinite(time)||time<0||time>trace.duration||!Number.isInteger(index)||
    index<0||index>=trace.segments.length/15)throw new RangeError('Invalid segment sample');
  const a=trace.segments.slice(index*15,index*15+15);
  const [t0,duration,ramp,peak,accel,slack,length,x0,y0,ux,uy,mx0,my0,line,frame]=a;
  const complete=time>=t0+duration;
  const t=complete?duration:Math.max(0,Math.min(duration,time-t0)),travel=length+slack;
  let distance,velocity;
  if(t>=duration){distance=travel;velocity=0;}
  else if(t<ramp){velocity=accel*t;distance=velocity*t/2;}
  else if(duration-t<ramp){velocity=accel*(duration-t);distance=travel-velocity*(duration-t)/2;}
  else {velocity=peak;distance=peak*(t-ramp/2);}
  const drawn=t>=duration?length:Math.max(0,Math.min(length,distance-slack));
  const s=trace.settings;
  return {x:quantize(x0+ux*drawn,s.steps_per_mm[0],s.width_mm),
    y:quantize(y0+uy*drawn,s.steps_per_mm[1],s.height_mm),
    mx:mx0+ux*distance,my:my0+uy*distance,velocity,
    backlash:distance<slack&&t<duration,line,frame,segment:index,drawn};
}
export function sampleTrace(trace,time) {
  if(!Number.isFinite(time)||time<0||time>trace.duration)throw new RangeError('Invalid physical sample time');
  let lo=0,hi=trace.segments.length/15;
  while(lo<hi){const mid=(lo+hi)>>1;if(trace.segments[mid*15]<=time)lo=mid+1;else hi=mid;}
  const point=lo?sampleSegment(trace,lo-1,time):{x:trace.start[0],y:trace.start[1],
    mx:trace.start[0],my:trace.start[1],velocity:0,backlash:false,line:0,frame:0,segment:-1,drawn:0};
  point.erase=trace.erases.find(e=>time>=e[0]&&time<e[0]+e[1])||null;
  return point;
}
