export interface Camera {
  x: number; // world-space center
  y: number;
  zoom: number;
}

export function createCamera(): Camera {
  return { x: 0, y: 0, zoom: 1 };
}

// screen coords -> world coords
export function screenToWorld(cam: Camera, sx: number, sy: number, cw: number, ch: number) {
  return {
    x: (sx - cw / 2) / cam.zoom + cam.x,
    y: (sy - ch / 2) / cam.zoom + cam.y,
  };
}

// apply camera transform to ctx
export function applyCamera(ctx: CanvasRenderingContext2D, cam: Camera, cw: number, ch: number) {
  ctx.translate(cw / 2, ch / 2);
  ctx.scale(cam.zoom, cam.zoom);
  ctx.translate(-cam.x, -cam.y);
}

export function setupCameraControls(
  canvas: HTMLCanvasElement,
  cam: Camera,
  callbacks: {
    onMouseDown?: (wx: number, wy: number, e: MouseEvent) => void;
    onMouseMove?: (wx: number, wy: number, sx: number, sy: number, e: MouseEvent) => void;
    onMouseUp?: () => void;
  },
) {
  let isPanning = false;
  let lastX = 0;
  let lastY = 0;

  canvas.addEventListener("mousedown", (e) => {
    const rect = canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const cw = rect.width;
    const ch = rect.height;
    const { x: wx, y: wy } = screenToWorld(cam, sx, sy, cw, ch);

    // middle mouse or shift+left = pan
    if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
      isPanning = true;
      lastX = e.clientX;
      lastY = e.clientY;
      e.preventDefault();
      return;
    }

    callbacks.onMouseDown?.(wx, wy, e);
  });

  window.addEventListener("mousemove", (e) => {
    if (isPanning) {
      const dx = e.clientX - lastX;
      const dy = e.clientY - lastY;
      cam.x -= dx / cam.zoom;
      cam.y -= dy / cam.zoom;
      lastX = e.clientX;
      lastY = e.clientY;
      return;
    }

    const rect = canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const cw = rect.width;
    const ch = rect.height;
    const { x: wx, y: wy } = screenToWorld(cam, sx, sy, cw, ch);
    callbacks.onMouseMove?.(wx, wy, sx, sy, e);
  });

  window.addEventListener("mouseup", () => {
    if (isPanning) {
      isPanning = false;
      return;
    }
    callbacks.onMouseUp?.();
  });

  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const cw = rect.width;
    const ch = rect.height;

    // world pos under cursor before zoom
    const before = screenToWorld(cam, sx, sy, cw, ch);

    const factor = e.deltaY < 0 ? 1.1 : 1 / 1.1;
    cam.zoom = Math.max(0.1, Math.min(10, cam.zoom * factor));

    // world pos under cursor after zoom
    const after = screenToWorld(cam, sx, sy, cw, ch);

    // adjust so the point under cursor stays fixed
    cam.x -= after.x - before.x;
    cam.y -= after.y - before.y;
  }, { passive: false });

  // prevent context menu on canvas
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());
}
