/* G2B AIRDRUMS — handtracking-worker (MediaPipe HandLandmarker, Apache-2.0).
   Draait het model buiten de hoofddraad zodat slagdetectie en audio nooit haperen.
   Levert per frame max. 2 handen met 21 landmark-posities (genormaliseerd 0..1). */
self.exports = {};
importScripts('./vendor/mediapipe/vision_bundle.cjs.js');
const { FilesetResolver, HandLandmarker } = self.exports;

let landmarker = null;
let ready = false;

async function createLandmarker(delegate) {
  const wasmBase = new URL('./vendor/mediapipe/wasm', self.location.href).href;
  const modelPath = new URL('./vendor/mediapipe/hand_landmarker.task', self.location.href).href;
  const vision = await FilesetResolver.forVisionTasks(wasmBase);
  const options = {
    baseOptions: {
      modelAssetPath: modelPath,
      delegate: delegate === 'GPU' ? 'GPU' : 'CPU'
    },
    runningMode: 'VIDEO',
    numHands: 2,
    minHandDetectionConfidence: 0.5,
    minHandPresenceConfidence: 0.5,
    minTrackingConfidence: 0.5
  };
  if (delegate === 'GPU' && typeof OffscreenCanvas !== 'undefined') {
    options.canvas = new OffscreenCanvas(224, 224);
  }
  return HandLandmarker.createFromOptions(vision, options);
}

async function init(delegate) {
  let used = 'CPU';
  if (delegate === 'AUTO') {
    try {
      landmarker = await createLandmarker('GPU');
      used = 'GPU';
    } catch (_) {
      landmarker = await createLandmarker('CPU');
      used = 'CPU';
    }
  } else {
    used = delegate === 'GPU' ? 'GPU' : 'CPU';
    landmarker = await createLandmarker(used);
  }
  try {
    if (typeof OffscreenCanvas !== 'undefined' && landmarker && landmarker.detectForVideo) {
      const warm = new OffscreenCanvas(224, 224);
      const wctx = warm.getContext('2d');
      wctx.fillStyle = '#000';
      wctx.fillRect(0, 0, 224, 224);
      landmarker.detectForVideo(warm, 0);
    }
  } catch (_) {}
  ready = true;
  postMessage({ type: 'ready', delegate: used });
}

self.onmessage = event => {
  const msg = event.data || {};
  (async () => {
    try {
      if (msg.type === 'init') {
        await init(msg.delegate || 'AUTO');
        return;
      }
      if (msg.type === 'close') {
        if (landmarker && landmarker.close) landmarker.close();
        ready = false;
        close();
        return;
      }
      if (msg.type !== 'detect') return;
      const bitmap = msg.image;
      if (!ready || !landmarker || !bitmap) {
        if (bitmap && bitmap.close) bitmap.close();
        postMessage({ type: 'result', seq: msg.seq, ok: false, error: 'handworker niet klaar' });
        return;
      }
      const start = performance.now();
      const result = landmarker.detectForVideo(bitmap, msg.timestamp);
      const inferenceMs = performance.now() - start;
      if (bitmap.close) bitmap.close();
      const hands = [];
      const lm = result && result.landmarks ? result.landmarks : [];
      const hd = result && result.handedness ? result.handedness : [];
      for (let h = 0; h < lm.length && h < 2; h++) {
        const pts = new Float32Array(21 * 2);
        for (let i = 0; i < 21; i++) {
          pts[i * 2] = lm[h][i].x;
          pts[i * 2 + 1] = lm[h][i].y;
        }
        let label = '';
        try { label = (hd[h] && hd[h][0] && hd[h][0].categoryName) || ''; } catch (_) {}
        hands.push({ pts: pts.buffer, handedness: label });
      }
      const transfers = hands.map(h => h.pts);
      postMessage({ type: 'result', seq: msg.seq, ok: true, timestamp: msg.timestamp, inferenceMs, hands }, transfers);
    } catch (error) {
      if (msg.image && msg.image.close) {
        try { msg.image.close(); } catch (_) {}
      }
      postMessage({ type: msg.type === 'init' ? 'init-error' : 'result', seq: msg.seq, ok: false, error: String(error && error.message || error) });
    }
  })();
};
