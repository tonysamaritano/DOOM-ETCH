// SPDX-License-Identifier: GPL-2.0-or-later
import assert from 'node:assert/strict';
import {decodeFrame} from '../platform/darwin/etch_play/decoder.js';
const game={version:1,sequence:1,kind:'indexed',width:320,height:200,mode:'game'};
const visual={...game,kind:'bgra',width:768,height:576,mode:'visual'};
const physical={...game,mode:'physical',cursor_mm:[10,20],motor_mm:[-0.15,130],
  physical_seconds:1.25,knob_mm_turn:20,travel_mm:[170.15,127.6]};
function packet(meta,body) {
  const json=new TextEncoder().encode(JSON.stringify(meta));
  body??=new Uint8Array(meta?.kind==='bgra'?768*576*4:320*200+1024);
  const bytes=new Uint8Array(4+json.length+body.length);
  new DataView(bytes.buffer).setUint32(0,json.length,true);
  bytes.set(json,4);bytes.set(body,4+json.length);return bytes.buffer;
}
assert.equal(decodeFrame(new ArrayBuffer(0)),null);
const indexed=new Uint8Array(65024);indexed[0]=17;indexed[63999]=31;
indexed.set([11,22,33,255],64000+17*4);
indexed.set([44,55,66,255],64000+31*4);
const first=decodeFrame(packet(game,indexed));
assert.deepEqual(first.metadata,game);
assert.equal(first.pixels.length,64000);assert.equal(first.palette.length,1024);
assert.equal(first.pixels[0],17);assert.equal(first.pixels.at(-1),31);
assert.deepEqual([...first.palette.slice(17*4,17*4+4)],[11,22,33,255]);
const color=new Uint8Array(768*576*4);
color.set([11,22,33,255]);color.set([77,88,99,255],color.length-4);
const second=decodeFrame(packet(visual,color));
assert.equal(second.palette,null);
assert.deepEqual([...second.pixels.slice(0,4)],[11,22,33,255]);
assert.deepEqual([...second.pixels.slice(-4)],[77,88,99,255]);
assert.deepEqual(decodeFrame(packet(physical)).metadata,physical);
