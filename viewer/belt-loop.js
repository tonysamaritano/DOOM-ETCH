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
