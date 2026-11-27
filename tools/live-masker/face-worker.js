/* G2B Live-Masker browserlijn — lokale gezichtstracking-worker.
   MediaPipe Tasks Vision Apache-2.0; model en WASM staan in ./vendor/mediapipe/. */
self.exports = {};
importScripts('./vendor/mediapipe/vision_bundle.cjs.js');
const { FilesetResolver, FaceLandmarker } = self.exports;

let landmarker = null;
let ready = false;
const WANTED_BLENDSHAPES = new Set([
  'jawOpen', 'mouthSmileLeft', 'mouthSmileRight', 'eyeBlinkLeft', 'eyeBlinkRight',
  'browInnerUp', 'mouthPucker', 'mouthClose'
]);

async function createLandmarker(delegate) {
  const wasmBase = new URL('./vendor/mediapipe/wasm', self.location.href).href;
  const modelPath = new URL('./vendor/mediapipe/face_landmarker.task', self.location.href).href;
  const vision = await FilesetResolver.forVisionTasks(wasmBase);
  return FaceLandmarker.createFromOptions(vision, {
    baseOptions: {
      modelAssetPath: modelPath,
      delegate: delegate === 'GPU' ? 'GPU' : 'CPU'
    },
    runningMode: 'VIDEO',
    numFaces: 1,
    minFaceDetectionConfidence: 0.5,
    minFacePresenceConfidence: 0.5,
    minTrackingConfidence: 0.5,
    outputFaceBlendshapes: true,
    outputFacialTransformationMatrixes: true
  });
}

async function init(delegate) {
  let usedDelegate = delegate === 'GPU' ? 'GPU' : 'CPU';
  if (delegate === 'AUTO') {
    try {
      landmarker = await createLandmarker('GPU');
      usedDelegate = 'GPU';
    } catch (_) {
      landmarker = await createLandmarker('CPU');
      usedDelegate = 'CPU';
    }
  } else {
    landmarker = await createLandmarker(usedDelegate);
  }
  try {
    if (typeof OffscreenCanvas !== 'undefined' && landmarker && landmarker.detectForVideo) {
      const warm = new OffscreenCanvas(192, 192);
      const warmCtx = warm.getContext('2d');
      warmCtx.fillStyle = '#000';
      warmCtx.fillRect(0, 0, warm.width, warm.height);
      landmarker.detectForVideo(warm, 0);
    }
  } catch (_) {}
  ready = true;
  postMessage({ type: 'ready', delegate: usedDelegate, numFaces: 1 });
}

function simplify(result) {
  const src = result && result.faceLandmarks && result.faceLandmarks[0];
  const blend = {};
  const cats = result && result.faceBlendshapes && result.faceBlendshapes[0] && result.faceBlendshapes[0].categories;
  if (cats) {
    for (const c of cats) {
      if (WANTED_BLENDSHAPES.has(c.categoryName)) blend[c.categoryName] = c.score;
    }
  }
  const matrixObj = result && result.facialTransformationMatrixes && result.facialTransformationMatrixes[0];
  const matrix = matrixObj && matrixObj.data ? Array.from(matrixObj.data) : (Array.isArray(matrixObj) ? matrixObj.slice(0, 16) : []);
  return {
    landmarks: src ? src.map(p => ({ x: p.x, y: p.y, z: p.z })) : [],
    blend,
    matrix
  };
}

self.onmessage = async event => {
  const msg = event.data || {};
  try {
    if (msg.type === 'init') {
      await init(msg.delegate);
      return;
    }
    if (msg.type === 'close') {
      if (landmarker) landmarker.close();
      ready = false;
      close();
      return;
    }
    if (msg.type !== 'detect') return;
    const bitmap = msg.image;
    if (!ready || !landmarker || !bitmap) {
      if (bitmap && bitmap.close) bitmap.close();
      postMessage({ type: 'result', seq: msg.seq, ok: false, error: 'worker niet klaar' });
      return;
    }
    const start = performance.now();
    const result = landmarker.detectForVideo(bitmap, msg.timestamp);
    const inferenceMs = performance.now() - start;
    if (bitmap.close) bitmap.close();
    const simple = simplify(result);
    postMessage({
      type: 'result',
      seq: msg.seq,
      ok: true,
      timestamp: msg.timestamp,
      inferenceMs,
      hasFace: simple.landmarks.length > 100,
      landmarks: simple.landmarks,
      blend: simple.blend,
      matrix: simple.matrix
    });
  } catch (error) {
    if (msg.image && msg.image.close) {
      try { msg.image.close(); } catch (_) {}
    }
    postMessage({ type: msg.type === 'init' ? 'init-error' : 'result', seq: msg.seq, ok: false, error: String(error && error.message || error) });
  }
};
