// SPDX-License-Identifier: GPL-2.0-or-later
import assert from 'node:assert/strict';
import {pathToFileURL} from 'node:url';
const modulePath = process.argv[2];
const {fields, validateTrace, sampleTrace, sampleSegment, quantize} =
  await import(pathToFileURL(modulePath));
const hand = {
  format: 'doom-etch-trace', version: 1, segment_fields: fields,
  settings: {width_mm: 170.15, height_mm: 127.6,
    steps_per_mm: [20, 20], knob_mm_turn: 20},
  start: [0, 0], duration: 143 / 75, simulated_seconds: 143 / 75,
  segments: [0, 143 / 75, .24, 60, 250, 0, 100, 0, 0, 1, 0, 0, 0, 1, 0],
  erases: [],
};
validateTrace(hand);
for (const time of [NaN, Infinity, -Infinity, -.001, hand.duration + .001]) {
  assert.throws(() => sampleTrace(hand, time), RangeError);
  assert.throws(() => sampleSegment(hand, 0, time), RangeError);
}
for (const index of [-1, .5, 1, NaN])
  assert.throws(() => sampleSegment(hand, index, 0), RangeError);
for (const args of [[NaN, 20, 1], [0, 0, 1], [0, 20, Infinity]])
  assert.throws(() => quantize(...args), RangeError);
console.log('viewer trace invalid sampling arguments rejected');
