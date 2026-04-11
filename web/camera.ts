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
    onMouseDown?: (wx: number, wy: number, e: MouseEvent) => boolean; // return true if handled (e.g. drag)
    onMouseMove?: (wx: number, wy: number, sx: number, sy: number, e: MouseEvent) => void;
    onMouseUp?: () => void;
  },
) {
  let isPanning = false;
  let lastX = 0;
  let lastY = 0;

  function updateCursor(e: MouseEvent | KeyboardEvent) {
    if (isPanning) {
      canvas.style.cursor = "grabbing";
    } else if ("shiftKey" in e && e.shiftKey) {
      canvas.style.cursor = "grab";
    }
  }

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
      canvas.style.cursor = "move";
      e.preventDefault();
      return;
    }

    // let callback handle it (node drag); if not handled, start panning
    if (e.button === 0) {
      const handled = callbacks.onMouseDown?.(wx, wy, e) ?? false;
      if (!handled) {
        isPanning = true;
        lastX = e.clientX;
        lastY = e.clientY;
        canvas.style.cursor = "move";
      } else {
        canvas.style.cursor = "grabbing";
      }
    }
  });

  window.addEventListener("mousemove", (e) => {
    if (isPanning) {
      const dx = e.clientX - lastX;
      const dy = e.clientY - lastY;
      cam.x -= dx / cam.zoom;
      cam.y -= dy / cam.zoom;
      lastX = e.clientX;
      lastY = e.clientY;
      canvas.style.cursor = "move";
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
    const wasPanning = isPanning;
    isPanning = false;
    if (!wasPanning) {
      callbacks.onMouseUp?.();
    }
    canvas.style.cursor = "crosshair";
  });

  // shift key cursor feedback
  window.addEventListener("keydown", (e) => {
    if (e.key === "Shift") canvas.style.cursor = "move";
  });
  window.addEventListener("keyup", (e) => {
    if (e.key === "Shift" && !isPanning) canvas.style.cursor = "crosshair";
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

    // trackpad pinch-zoom fires with ctrlKey and small deltaY values;
    // use a gentler sensitivity for those events
    const sensitivity = e.ctrlKey ? 0.01 : 0.002;
    const factor = Math.pow(2, -e.deltaY * sensitivity);
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
