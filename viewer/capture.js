// SPDX-License-Identifier: GPL-2.0-or-later
const FRAMES = 540;
const WIDTH = 1920;
const HEIGHT = 1080;
async function request(path, token, frame, body) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 30000);
  try {
    const response = await fetch(path, {
      method: 'POST', signal: controller.signal,
      headers: body ? {'Content-Type': 'image/jpeg'} : {}, body,
    });
    if (!response.ok) throw new Error(`Capture HTTP ${response.status}`);
    const ack = await response.json();
    if (ack.ok !== true || ack.cut !== 'pitch' ||
        typeof ack.token !== 'string' || !ack.token ||
        (token && ack.token !== token) ||
        (frame !== undefined && ack.frame !== frame)) {
      throw new Error('Invalid capture acknowledgment');
    }
    return ack;
  } finally {
    clearTimeout(timeout);
  }
}
function jpeg(canvas) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('JPEG encoding timed out')), 30000);
    canvas.toBlob(blob => {
      clearTimeout(timeout);
      if (!blob || blob.type !== 'image/jpeg' || !blob.size) {
        reject(new Error('JPEG encoding failed'));
      } else resolve(blob);
    }, 'image/jpeg', .94);
  });
}
// Caller has loaded assets/fonts and yielded the sole render pump to capture.
export async function startCapture({renderAt, setSize, canvas, onProgress = () => {}}) {
  let frame = 0;
  let completed = 0;
  const progress = (state, error) => onProgress({state, frame, completed, total: FRAMES,
    ...(error ? {error: String(error.message || error)} : {})});
  try {
    if (typeof renderAt !== 'function' || typeof setSize !== 'function' ||
        !canvas || typeof canvas.toBlob !== 'function') throw new Error('Missing capture adapter');
    progress('starting');
    setSize(WIDTH, HEIGHT);
    if (canvas.width !== WIDTH || canvas.height !== HEIGHT) throw new Error('Capture must be 1920x1080');
    const start = await request('/__start?cut=pitch');
    const token = start.token;
    for (frame = 0; frame < FRAMES; frame++) {
      renderAt(frame / 30);
      if (canvas.width !== WIDTH || canvas.height !== HEIGHT) throw new Error('Capture size changed');
      const blob = await jpeg(canvas);
      await request(`/__frame?cut=pitch&token=${encodeURIComponent(token)}&frame=${frame}`,
        token, frame, blob);
      completed++;
      progress('capturing');
    }
    const done = await request(`/__done?cut=pitch&token=${encodeURIComponent(token)}`, token);
    if (done.frames !== FRAMES) throw new Error('Incomplete capture acknowledgment');
    progress('done');
    return {frames: FRAMES};
  } catch (error) {
    progress('error', error);
    throw error;
  }
}
