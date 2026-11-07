// SPDX-License-Identifier: GPL-2.0-or-later
import assert from 'node:assert/strict';
import test from 'node:test';
import {pitchContext,buildPitchState,layoutPitch,drawPitchOverlay,createPitch,
  pitchOptions} from '../viewer/promo.js';
function fixture() {
  return {duration:30,erases:[],settings:{assumed:true,knob_mm_turn:20},
    segments:[0,24,1,1,1,0,24,0,0,1,0,0,0,1,0,
      24,6,1,1,1,0,6,24,0,1,0,24,0,2,0],
    requirements:[{type:'machine-requirements',frame:0,target_fps:30,
      geometry:'commanded-step-grid',budget:'drawing-only-one-frame',path_m:.03,
      no_stop_m_s:.9,no_stop_mach:.9/343,accel_g:36,peak_mach:.01,knob_rpm:2700,
      assumptions:{sound_speed_m_s:343,gravity_m_s2:9.80665,knob_mm_turn:20,
        knob_rpm:'path-speed-equivalent'},exclusions:['backlash','shake','jerk']}]};
}
function measure(text,font) {
  const size=Number(font.match(/([\d.]+)px/)[1]),width=text.length*size*.6;
  return {width,actualBoundingBoxLeft:width*.55,actualBoundingBoxRight:width*.45,
    actualBoundingBoxAscent:size*.8,actualBoundingBoxDescent:size*.2};
}
test('pitch rejects unsubstantiated calibration claims',()=>{
  for(const mutate of [t=>delete t.requirements[0].assumptions,
    t=>t.requirements[0].assumptions.knob_mm_turn=10,
    t=>t.requirements[0].assumptions.sound_speed_m_s=1,
    t=>t.settings.assumed=false]) {
    const trace=fixture();mutate(trace);assert.throws(()=>pitchContext(trace));
  }
});
test('overlay preserves the already rendered drawing',()=>{
  const state=buildPitchState(pitchContext(fixture()),.5),calls=[];
  drawPitchOverlay({clearRect(...args){calls.push(args);}},state,
    layoutPitch(state,1920,1080,measure));
  assert.deepEqual(calls,[],'overlay must not erase the composited rig');
});
test('timeline boundaries, endpoint and selected drive interval are exact',()=>{
  const context=pitchContext(fixture());
  for(const [t,shot] of [[0,'bezel'],[3,'rig'],[6,'screen'],[9,'drive'],
    [12.5,'speed'],[15,'price'],[18,'price']]) {
    assert.equal(buildPitchState(context,t).shot,shot);
    if(t&&t!==18)assert.notEqual(buildPitchState(context,t-1e-8).shot,shot);
  }
  for(const t of [9,9.5,10.5,12.499]) {
    const state=buildPitchState(context,t);
    assert.equal(state.physical_time,context.interval.start+t-9);
    assert.ok(state.physical_time<context.duration);
  }
  for(const t of [0,3,6,12.5,15,18])
    assert.equal(buildPitchState(context,t).physical_time,30);
  for(const t of [-1,18.01,NaN,Infinity])assert.throws(()=>buildPitchState(context,t));
});
test('production renderAt derives complete state independent of call order',()=>{
  let size=[1920,1080];const paints=[];
  const pitch=createPitch(fixture(),{size:()=>size,measure,
    paint:(state,layout)=>{paints.push(state.physical_time);return [layout.width,state.shot];}});
  const first=pitch.renderAt(16.5);pitch.renderAt(10.5);
  assert.deepEqual(pitch.renderAt(16.5),first);
  assert.deepEqual(paints,[30,25.5,30]);
  size=[360,800];assert.equal(pitch.renderAt(16.5).layout.width,360);
  size=[1920,1080];assert.deepEqual(pitch.renderAt(16.5),first);
});
