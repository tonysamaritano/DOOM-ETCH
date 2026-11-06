const physicalFields=['cursor_mm','motor_mm','physical_seconds','knob_mm_turn','travel_mm'];
function pair(value,positive=false) {
  return Array.isArray(value)&&value.length===2&&value.every(x=>Number.isFinite(x)&&(!positive||x>0));
}
function metadataValid(m) {
  if(!m||typeof m!=='object'||Array.isArray(m)||m.version!==1||
    !Number.isSafeInteger(m.sequence)||m.sequence<=0)throw Error('Invalid frame version or sequence');
  if(!['visual','physical','game'].includes(m.mode))throw Error('Invalid frame mode');
  const indexed=m.kind==='indexed',bgra=m.kind==='bgra';
  if((!indexed&&!bgra)||m.width!==(indexed?320:768)||m.height!==(indexed?200:576))
    throw Error('Invalid packed pixel dimensions or kind');
  if((m.mode==='game')!==indexed)throw Error('Frame kind does not match mode');
  if(m.mode==='physical') {
    if(!pair(m.cursor_mm)||!pair(m.motor_mm)||!pair(m.travel_mm,true)||
      !Number.isFinite(m.physical_seconds)||m.physical_seconds<0||
      !Number.isFinite(m.knob_mm_turn)||m.knob_mm_turn<=0)throw Error('Invalid physical frame state');
    if(m.cursor_mm.some((x,i)=>x<0||x>m.travel_mm[i]))throw Error('Cursor outside physical travel');
  } else if(physicalFields.some(key=>Object.hasOwn(m,key)))throw Error('Physical state on a nonphysical frame');
}
