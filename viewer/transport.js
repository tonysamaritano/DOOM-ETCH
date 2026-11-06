export function buildTransport(trace) {
  const play=document.getElementById('play'),restart=document.getElementById('restart'),seek=document.getElementById('seek');
  const parameter=new URLSearchParams(location.search).get('t'),initial=parameter===null?0:Number(parameter);
  if(!Number.isFinite(initial)||initial<0)throw Error('The t parameter must be finite physical seconds');
  const state={time:Math.min(trace.duration,initial),playing:false,last:performance.now()};
  play.disabled=trace.duration===0;restart.disabled=false;seek.disabled=trace.duration===0;seek.max=trace.duration;
  play.onclick=()=>{if(state.time>=trace.duration)state.time=0;state.playing=!state.playing;state.last=performance.now();};
  restart.onclick=()=>{state.time=0;state.playing=false;};
  seek.oninput=()=>{state.time=Number(seek.value);state.playing=false;};
  document.addEventListener('visibilitychange',()=>{if(document.hidden)state.playing=false;state.last=performance.now();});
  return {state,tick(now){const elapsed=Math.max(0,(now-state.last)/1000);state.last=now;
    if(state.playing){state.time=Math.min(trace.duration,state.time+elapsed);if(state.time===trace.duration)state.playing=false;}
    play.textContent=state.playing?'Pause':'Play';seek.value=state.time;
    document.getElementById('time').textContent=state.time.toFixed(3);
    document.getElementById('duration').textContent='/ '+trace.duration.toFixed(3);
    return state.time;
  }};
}
