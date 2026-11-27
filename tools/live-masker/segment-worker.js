/* G2B Live-Masker browserlijn — lichte persoonssegmentatie-worker voor achtergrondmagie.
   MediaPipe Tasks Vision Apache-2.0; selfie_segmenter.tflite staat lokaal in ./vendor/mediapipe/. */
self.exports = {};
importScripts('./vendor/mediapipe/vision_bundle.cjs.js');
const { FilesetResolver, ImageSegmenter } = self.exports;

let segmenter = null;
let ready = false;

async function createSegmenter(delegate) {
  const wasmBase = new URL('./vendor/mediapipe/wasm', self.location.href).href;
  const modelPath = new URL('./vendor/mediapipe/selfie_segmenter.tflite', self.location.href).href;
  const vision = await FilesetResolver.forVisionTasks(wasmBase);
  const options = {
    baseOptions: {
      modelAssetPath: modelPath,
      delegate: delegate === 'GPU' ? 'GPU' : 'CPU'
    },
    runningMode: 'VIDEO',
    outputCategoryMask: false,
    outputConfidenceMasks: true
  };
  if (delegate === 'GPU' && typeof OffscreenCanvas !== 'undefined') {
    options.canvas = new OffscreenCanvas(256, 256);
  }
  return ImageSegmenter.createFromOptions(vision, options);
}

async function init(delegate) {
  let used = 'CPU';
  if (delegate === 'AUTO') {
    try {
      segmenter = await createSegmenter('GPU');
      used = 'GPU';
    } catch (_) {
      segmenter = await createSegmenter('CPU');
      used = 'CPU';
    }
  } else {
    used = delegate === 'GPU' ? 'GPU' : 'CPU';
    segmenter = await createSegmenter(used);
  }
  try {
    if (typeof OffscreenCanvas !== 'undefined' && segmenter && segmenter.segmentForVideo) {
      const warm = new OffscreenCanvas(256, 256);
      const wctx = warm.getContext('2d');
      wctx.fillStyle = '#000';
      wctx.fillRect(0, 0, 256, 256);
      segmenter.segmentForVideo(warm, 0);
    }
  } catch (_) {}
  ready = true;
  postMessage({ type: 'ready', delegate: used });
}

function quantize(mask) {
  const src = mask.getAsFloat32Array();
  const width = mask.width || 256;
  const height = mask.height || Math.max(1, Math.round(src.length / width));
  const out = new Uint8Array(src.length);
  let confident = 0;
  for (let i = 0; i < src.length; i++) {
    const v = Math.max(0, Math.min(1, src[i]));
    const q = Math.round(v * 255);
    out[i] = q;
    if (q > 90) confident++;
  }
  return { width, height, data: out, coverage: confident / Math.max(1, src.length) };
}

self.onmessage = async event => {
  const msg = event.data || {};
  try {
    if (msg.type === 'init') {
      await init(msg.delegate || 'AUTO');
      return;
    }
    if (msg.type === 'close') {
      if (segmenter && segmenter.close) segmenter.close();
      ready = false;
      close();
      return;
    }
    if (msg.type !== 'segment') return;
    const bitmap = msg.image;
    if (!ready || !segmenter || !bitmap) {
      if (bitmap && bitmap.close) bitmap.close();
      postMessage({ type: 'result', seq: msg.seq, ok: false, error: 'segmentworker niet klaar' });
      return;
    }
    const start = performance.now();
    const result = segmenter.segmentForVideo(bitmap, msg.timestamp);
    const inferenceMs = performance.now() - start;
    if (bitmap.close) bitmap.close();
    const masks = result && result.confidenceMasks ? result.confidenceMasks : [];
    if (!masks.length) throw new Error('geen confidence mask');
    const mask = quantize(masks[masks.length - 1]);
    if (result && result.close) result.close();
    postMessage({
      type: 'result', seq: msg.seq, ok: true, timestamp: msg.timestamp, inferenceMs,
      width: mask.width, height: mask.height, mask: mask.data.buffer,
      personCoverage: mask.coverage
    }, [mask.data.buffer]);
  } catch (error) {
    if (msg.image && msg.image.close) {
      try { msg.image.close(); } catch (_) {}
    }
    postMessage({ type: msg.type === 'init' ? 'init-error' : 'result', seq: msg.seq, ok: false, error: String(error && error.message || error) });
  }
};
