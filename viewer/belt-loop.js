const tau=2*Math.PI;
export function pitchLoop(big,small,centers) {
  if(!(big>small&&centers>big-small))throw Error('Invalid pulley pitch geometry');
  const sine=(big-small)/centers,cosine=Math.sqrt(1-sine*sine),beta=Math.asin(sine);
  const angle=Math.PI/2-beta,arcBig=big*(Math.PI+2*beta),straight=centers*cosine;
  const arcSmall=small*(Math.PI-2*beta),length=arcBig+2*straight+arcSmall;
  function point(distance) {
    let u=((distance%length)+length)%length;
    if(u<arcBig){const a=angle+u/big;return [big*Math.cos(a),big*Math.sin(a),Math.cos(a),Math.sin(a)];}
    u-=arcBig;
    if(u<straight)return [big*sine+u*cosine,-big*cosine+u*sine,sine,-cosine];
    u-=straight;
    if(u<arcSmall){const a=-angle+u/small;return [centers+small*Math.cos(a),small*Math.sin(a),Math.cos(a),Math.sin(a)];}
    u-=arcSmall;
    return [centers+small*sine-u*cosine,small*cosine+u*sine,sine,cosine];
  }
  return {big,small,centers,sine,cosine,angle,arcBig,straight,arcSmall,length,point};
}
export function projectLoop(loop,x,y) {
  const b=loop,candidates=[];
  function arc(cx,radius,start,sweep,offset) {
    const a=start+((Math.atan2(y,x-cx)-start)%tau+tau)%tau;
    if(a<=start+sweep)candidates.push(offset+radius*(a-start));
  }
  arc(0,b.big,b.angle,Math.PI+2*Math.asin(b.sine),0);
  arc(b.centers,b.small,-b.angle,2*b.angle,b.arcBig+b.straight);
  const lower=(x-b.big*b.sine)*b.cosine+(y+b.big*b.cosine)*b.sine;
  candidates.push(b.arcBig+Math.max(0,Math.min(b.straight,lower)));
  const upper=-(x-b.centers-b.small*b.sine)*b.cosine+(y-b.small*b.cosine)*b.sine;
  candidates.push(b.arcBig+b.straight+b.arcSmall+Math.max(0,Math.min(b.straight,upper)));
  let best=null;
  for(const u of candidates) {
    const p=b.point(u),dx=x-p[0],dy=y-p[1],error=dx*dx+dy*dy;
    if(!best||error<best.error)best={u,offset:dx*p[2]+dy*p[3],error};
  }
  return best;
}
