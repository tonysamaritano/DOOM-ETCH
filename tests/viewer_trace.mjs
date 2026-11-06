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
for (const [time, expected] of [[0, 0], [.12, 1.8], [.24, 7.2], [.5, 22.8], [143 / 75, 100]]) {
  const point = sampleTrace(hand, time);
  assert(Math.abs(point.x - expected) < 1e-12);
  assert(Math.abs(point.mx - expected) < 1e-12);
}
// Exact half-grid ties can differ at floating-point discontinuities. Compare
// either side of the independent x=.025 mm acceleration threshold instead.
const threshold = Math.sqrt(2 * .025 / 250);
assert.equal(sampleTrace(hand, threshold * (1 - 1e-8)).x, 0);
assert.equal(sampleTrace(hand, threshold * (1 + 1e-8)).x, .05);
for (const field of [3, 4]) {
  const invalid = structuredClone(hand);
  invalid.segments[field] = -1;
  assert.throws(() => validateTrace(invalid));
}
for (const invalid of [
  {...hand, duration: NaN},
  {...hand, simulated_seconds: Infinity},
  {...hand, start: [0, NaN]},
  {...hand, segments: [NaN, ...hand.segments.slice(1)]},
  {...hand, erases: [[0, Infinity]]},
]) assert.throws(() => validateTrace(invalid));

const {writeFileSync, readFileSync, mkdirSync} = await import('node:fs');
const {dirname} = await import('node:path');
const {spawnSync} = await import('node:child_process');
const generator = process.argv[3], prefix = process.argv[4];
assert(generator && prefix, 'Pass the native generator and scratch output prefix');
mkdirSync(dirname(prefix), {recursive: true});
const fixtures = [
  {name: 'straight', text: 'G1 X100 F3600\nM2\n'},
  {name: 'reverse', text: 'G1 X100 F3600\nG1 X0\nM2\n'},
  {name: 'asymmetric', settings: ['$100=40', '$101=16', '$111=420', '$120=47'],
    text: 'G1 X100 F3600\nG1 X40 Y97.6\nG1 X50 Y87.6 F600\n'
      + 'M0\nG0 X10 Y117.6\nG1 X10 Y117.6\nM0\nM2\n'},
  {name: 'clipped-extent',
    text: '; ETCH_SIZE X1.023 Y1.037\n; ETCH_START X0 Y1.037\n'
      + 'G1 X1.023 Y0 F3600\nG1 X0 Y1.037\nM0\nM2\n'},
  {name: 'stationary', text: '; ETCH_START X5 Y122.6\n'
      + 'G1 X5 Y122.6 F600\nM0\nG1 X5 Y122.6\nM2\n'},
  {name: 'empty', text: ''},
  {name: 'end-only', text: 'M2\n'},
  {name: 'erase-only', text: 'M0\nM0\nM2\n'},
];
for (const fixture of fixtures) {
  const stem = `${prefix}-${fixture.name}`;
  writeFileSync(`${stem}.gcode`, fixture.text);
  const result = spawnSync(generator,
    [`${stem}.gcode`, `${stem}.json`, `${stem}-samples.json`, ...(fixture.settings || [])],
    {encoding: 'utf8', timeout: 15000});
  assert.equal(result.status, 0, `${fixture.name}: ${result.stderr}`);
  const trace = validateTrace(JSON.parse(readFileSync(`${stem}.json`, 'utf8')));
  const samples = JSON.parse(readFileSync(`${stem}-samples.json`, 'utf8'));
  if (fixture.name === 'reverse') {
    const takeup = sampleTrace(trace, trace.segments[15] + .001);
    assert(takeup.backlash && takeup.x === 100 && takeup.mx < 100);
  }
  for (const erase of trace.erases)
    assert.deepEqual(sampleTrace(trace, erase[0] + erase[1] / 2).erase, erase);
  for (const [time, x, y, mx, my] of samples) {
    const point = sampleTrace(trace, time);
    for (const [key, expected] of Object.entries({x, y, mx, my}))
      assert(Math.abs(point[key] - expected) <= 1e-5,
        `${fixture.name} at ${time}: ${key}=${point[key]}, native=${expected}`);
  }
  console.log(`${fixture.name}: ${samples.length} native samples agree within 1e-5 mm`);
}
