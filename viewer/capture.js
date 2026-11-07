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
