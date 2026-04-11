// easy way to test if simulation is framerate indepedent
// 0 uses requestAnimationFrame, otherwise fixed timestep
const FIXED_FPS = 0;

let lastTime = performance.now();

export function startLoop(
  canvas: HTMLCanvasElement,
  tick: (ctx: CanvasRenderingContext2D, dt: number) => void,
) {
  if (FIXED_FPS) {
    setInterval(() => {
      runTickStep(canvas, tick);
    }, 1000 / FIXED_FPS);
  } else {
    startRafLoop(canvas, tick);
  }
}

function startRafLoop(
  canvas: HTMLCanvasElement,
  tick: (ctx: CanvasRenderingContext2D, dt: number) => void,
) {
  runTickStep(canvas, tick);
  requestAnimationFrame(() => startRafLoop(canvas, tick));
}

function runTickStep(
  canvas: HTMLCanvasElement,
  tick: (ctx: CanvasRenderingContext2D, dt: number) => void,
) {
  const now = performance.now();
  const dt = now - lastTime;
  lastTime = now;

  const canvasRect = canvas.getBoundingClientRect();
  canvas.width = canvasRect.width * devicePixelRatio;
  canvas.height = canvasRect.height * devicePixelRatio;

  const ctx = canvas.getContext("2d");
  assert(ctx);
  ctx.scale(devicePixelRatio, devicePixelRatio);

  tick(ctx, dt);
}

function assert(condition: any): asserts condition {
  if (!condition) throw new Error("Assertion failed");
}
