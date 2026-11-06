export const fields = ['t0','duration','ramp','peak','accel','slack','length',
  'x0','y0','ux','uy','motor_x0','motor_y0','line','frame'];
export function validateTrace(data) {
  if(data.format!=='doom-etch-trace'||data.version!==1||
    JSON.stringify(data.segment_fields)!==JSON.stringify(fields))throw Error('Unsupported trace schema');
  const s=data.settings;
  if(!s||![s.width_mm,s.height_mm,s.knob_mm_turn,...(s.steps_per_mm||[])].every(x=>Number.isFinite(x)&&x>0)||
    s.steps_per_mm.length!==2)throw Error('Invalid physical dimensions or calibration');
  if(!Array.isArray(data.start)||data.start.length!==2||!data.start.every(Number.isFinite)||
    !Number.isFinite(data.duration)||data.duration<0||data.duration!==data.simulated_seconds)
    throw Error('Invalid trace start or completed clock');
  if(!Array.isArray(data.segments)||data.segments.length%15||!data.segments.every(Number.isFinite))
    throw Error('Invalid segment records');
  if(!Array.isArray(data.erases)||!data.erases.every(e=>Array.isArray(e)&&e.length===2&&e.every(Number.isFinite)&&e[0]>=0&&e[1]>0))
    throw Error('Invalid erase records');
  for(let i=0;i<data.segments.length;i+=15) {
    const a=data.segments.slice(i,i+15);
    if(a[0]<0||a[1]<0||a[2]<0||a[5]<0||a[6]<0||a[0]+a[1]>data.duration+1e-9||
      (i&&a[0]<data.segments[i-15]))throw Error('Invalid segment timing');
  }
  return data;
}
export function quantize(value,steps,extent) {
  return Math.max(0,Math.min(Math.round(value*steps),Math.floor(extent*steps)))/steps;
}
