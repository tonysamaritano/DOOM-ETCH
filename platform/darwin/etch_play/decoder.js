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
  if((m.mode==='game'&&!indexed)||(m.mode==='visual'&&!bgra))throw Error('Frame kind does not match mode');
  if(m.mode==='physical') {
    if(!pair(m.cursor_mm)||!pair(m.motor_mm)||!pair(m.travel_mm,true)||
      !Number.isFinite(m.physical_seconds)||m.physical_seconds<0||
      !Number.isFinite(m.knob_mm_turn)||m.knob_mm_turn<=0)throw Error('Invalid physical frame state');
    if(m.cursor_mm.some((x,i)=>x<0||x>m.travel_mm[i]))throw Error('Cursor outside physical travel');
  } else if(physicalFields.some(key=>Object.hasOwn(m,key)))throw Error('Physical state on a nonphysical frame');
}
export function decodeFrame(buffer) {
  if(!(buffer instanceof ArrayBuffer))throw new TypeError('Frame must be an ArrayBuffer');
  if(buffer.byteLength===0)return null;
  if(buffer.byteLength<4)throw Error('Truncated frame header');
  const jsonBytes=new DataView(buffer).getUint32(0,true);
  if(!jsonBytes||jsonBytes>16384||jsonBytes>buffer.byteLength-4)throw Error('Invalid metadata length');
  const json=new TextDecoder('utf-8',{fatal:true}).decode(new Uint8Array(buffer,4,jsonBytes));
  const metadata=JSON.parse(json);metadataValid(metadata);
  const pixelBytes=metadata.width*metadata.height*(metadata.kind==='bgra'?4:1);
  const paletteBytes=metadata.kind==='indexed'?1024:0,offset=4+jsonBytes;
  if(buffer.byteLength!==offset+pixelBytes+paletteBytes)throw Error('Packed pixel length mismatch');
  const pixels=new Uint8Array(buffer,offset,pixelBytes);
  const palette=paletteBytes?new Uint8Array(buffer,offset+pixelBytes,paletteBytes):null;
  return {metadata,pixels,palette};
}
export function frameRGBA(frame) {
  const {metadata:m,pixels,palette}=frame;
  const rgba=new Uint8Array(m.width*m.height*4);
  if(m.kind==='bgra') {
    for(let i=0;i<rgba.length;i+=4) {
      rgba[i]=pixels[i+2];rgba[i+1]=pixels[i+1];rgba[i+2]=pixels[i];rgba[i+3]=pixels[i+3];
    }
  } else {
    for(let i=0;i<pixels.length;i++) {
      const color=pixels[i]*4,out=i*4;
      rgba[out]=palette[color];rgba[out+1]=palette[color+1];
      rgba[out+2]=palette[color+2];rgba[out+3]=palette[color+3];
    }
  }
  return rgba;
}
