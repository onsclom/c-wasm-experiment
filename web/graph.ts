import type { ASTNode } from "./parser";
import type { Camera } from "./camera";
import { screenToWorld } from "./camera";

// --- graph node / edge types ---

export interface GraphNode {
  ast: ASTNode;
  x: number;
  y: number;
  vx: number;
  vy: number;
  w: number; // rendered width
  h: number; // rendered height
}

interface Edge {
  from: number;
  to: number;
}

// --- simulation constants ---

const REPULSION = 3000;
const ATTRACTION = 0.08;
const IDEAL_EDGE_LEN = 90;
const MAX_EDGE_LEN = 250;
const DAMPING = 0.85;
const MAX_SPEED = 800;

const NODE_PAD_X = 20;
const NODE_PAD_Y = 12;
const NODE_FONT = "13px monospace";
const NODE_HIGHLIGHT = "#06c";

// --- measure node widths (needs a canvas ctx) ---

export function measureNodes(
  ctx: CanvasRenderingContext2D,
  nodes: GraphNode[],
) {
  ctx.font = NODE_FONT;
  for (const n of nodes) {
    const label = `${n.ast.type}: ${n.ast.text}`;
    n.w = ctx.measureText(label).width + NODE_PAD_X * 2;
  }
}

// --- build flat arrays from AST (radial layout) ---

// count leaves so we can proportion angular slices
function subtreeLeafCount(ast: ASTNode): number {
  if (ast.children.length === 0) return 1;
  let count = 0;
  for (const c of ast.children) count += subtreeLeafCount(c);
  return count;
}

const RADIAL_STEP = 100; // distance per depth level from center

export function buildGraph(root: ASTNode): {
  nodes: GraphNode[];
  edges: Edge[];
} {
  const nodes: GraphNode[] = [];
  const edges: Edge[] = [];

  // root goes at origin
  // each child gets an angular slice proportional to its leaf count
  // within that slice, grandchildren subdivide further

  function visit(
    ast: ASTNode,
    depth: number,
    angleStart: number, // radians
    angleEnd: number, // radians
  ) {
    const angle = (angleStart + angleEnd) / 2;
    const radius = depth * RADIAL_STEP;
    const idx = nodes.length;
    nodes.push({
      ast,
      x: Math.cos(angle) * radius,
      y: Math.sin(angle) * radius,
      vx: 0,
      vy: 0,
      w: 100,
      h: 13 + NODE_PAD_Y * 2,
    });

    if (ast.children.length === 0) return;

    const totalLeaves = subtreeLeafCount(ast);
    let currentAngle = angleStart;

    for (const child of ast.children) {
      const childLeaves = subtreeLeafCount(child);
      const childSlice = (childLeaves / totalLeaves) * (angleEnd - angleStart);
      const childIdx = nodes.length;
      visit(child, depth + 1, currentAngle, currentAngle + childSlice);
      edges.push({ from: idx, to: childIdx });
      currentAngle += childSlice;
    }
  }

  visit(root, 0, 0, Math.PI * 2);
  return { nodes, edges };
}

// --- simulation step (framerate independent) ---

export function simulate(
  nodes: GraphNode[],
  edges: Edge[],
  dt: number,
  dragIdx: number | null,
) {
  const dtS = Math.min(dt / 1000, 0.032);

  // repulsion between all pairs — use rectangle separation, not center distance
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const a = nodes[i];
      const b = nodes[j];

      // gap between rectangles (negative = overlapping)
      const halfWA = a.w / 2;
      const halfWB = b.w / 2;
      const halfHA = a.h / 2;
      const halfHB = b.h / 2;

      let dx = b.x - a.x;
      let dy = b.y - a.y;

      // effective distance: shrink by the combined half-sizes so we measure
      // edge-to-edge rather than center-to-center
      const overlapX = halfWA + halfWB;
      const overlapY = halfHA + halfHB;

      // use a "soft" distance that gets very small when rects overlap
      const gapX = Math.max(Math.abs(dx) - overlapX, 0);
      const gapY = Math.max(Math.abs(dy) - overlapY, 0);
      const gapDist = Math.sqrt(gapX * gapX + gapY * gapY);

      // use center distance for direction
      const centerDist = Math.sqrt(dx * dx + dy * dy) || 1;

      // force based on gap distance (strong when overlapping/close)
      const effectiveDist = Math.max(gapDist, 5);
      const force = REPULSION / (effectiveDist * effectiveDist);

      const fx = (dx / centerDist) * force;
      const fy = (dy / centerDist) * force;
      a.vx -= fx;
      a.vy -= fy;
      b.vx += fx;
      b.vy += fy;
    }
  }

  // attraction along edges
  for (const e of edges) {
    const a = nodes[e.from];
    const b = nodes[e.to];
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const dist = Math.sqrt(dx * dx + dy * dy) || 1;
    const force = (dist - IDEAL_EDGE_LEN) * ATTRACTION;
    const fx = (dx / dist) * force;
    const fy = (dy / dist) * force;
    a.vx += fx;
    a.vy += fy;
    b.vx -= fx;
    b.vy -= fy;
  }

  // integrate
  for (let i = 0; i < nodes.length; i++) {
    if (i === dragIdx) {
      nodes[i].vx = 0;
      nodes[i].vy = 0;
      continue;
    }
    nodes[i].vx *= DAMPING;
    nodes[i].vy *= DAMPING;
    const speed = Math.sqrt(nodes[i].vx ** 2 + nodes[i].vy ** 2);
    if (speed > MAX_SPEED) {
      nodes[i].vx = (nodes[i].vx / speed) * MAX_SPEED;
      nodes[i].vy = (nodes[i].vy / speed) * MAX_SPEED;
    }
    nodes[i].x += nodes[i].vx * dtS;
    nodes[i].y += nodes[i].vy * dtS;
  }

  // hard distance constraint on edges — if two connected nodes are further
  // than MAX_EDGE_LEN apart, pull them both toward each other (or just the
  // non-dragged one if one is being dragged)
  for (let iter = 0; iter < 3; iter++) {
    for (const e of edges) {
      const a = nodes[e.from];
      const b = nodes[e.to];
      const dx = b.x - a.x;
      const dy = b.y - a.y;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist <= MAX_EDGE_LEN) continue;

      const excess = dist - MAX_EDGE_LEN;
      const nx = dx / dist;
      const ny = dy / dist;

      const aIsDragged = e.from === dragIdx;
      const bIsDragged = e.to === dragIdx;

      if (aIsDragged) {
        // only move b
        b.x -= nx * excess;
        b.y -= ny * excess;
      } else if (bIsDragged) {
        // only move a
        a.x += nx * excess;
        a.y += ny * excess;
      } else {
        // split correction equally
        a.x += nx * (excess / 2);
        a.y += ny * (excess / 2);
        b.x -= nx * (excess / 2);
        b.y -= ny * (excess / 2);
      }
    }
  }
}

// --- pre-settle: run simulation ahead of time, then fit camera ---

export function settleAndFitCamera(
  nodes: GraphNode[],
  edges: Edge[],
  cam: Camera,
  canvasW: number,
  canvasH: number,
) {
  const settleTime = 1000;
  for (let i = 0; i < settleTime; i++) {
    simulate(nodes, edges, 16.67, null);
  }

  fitCamera(nodes, cam, canvasW, canvasH);
}

export function fitCamera(
  nodes: GraphNode[],
  cam: Camera,
  canvasW: number,
  canvasH: number,
) {
  if (nodes.length === 0) return;

  let minX = Infinity,
    maxX = -Infinity,
    minY = Infinity,
    maxY = -Infinity;

  for (const n of nodes) {
    minX = Math.min(minX, n.x - n.w / 2);
    maxX = Math.max(maxX, n.x + n.w / 2);
    minY = Math.min(minY, n.y - n.h / 2);
    maxY = Math.max(maxY, n.y + n.h / 2);
  }

  const padding = 40;
  const boundsW = maxX - minX + padding * 2;
  const boundsH = maxY - minY + padding * 2;

  cam.x = (minX + maxX) / 2;
  cam.y = (minY + maxY) / 2;
  cam.zoom = Math.min(canvasW / boundsW, canvasH / boundsH, 2);
}

// --- dot grid background ---

const DOT_COLOR = "#d0d0d0";
const DOT_RADIUS = 2;
const TARGET_SCREEN_SPACING = 32; // desired px between dots on screen

export function drawDotGrid(
  ctx: CanvasRenderingContext2D,
  cam: Camera,
  canvasW: number,
  canvasH: number,
) {
  // figure out visible world bounds
  const tl = screenToWorld(cam, 0, 0, canvasW, canvasH);
  const br = screenToWorld(cam, canvasW, canvasH, canvasW, canvasH);

  const rawStep = TARGET_SCREEN_SPACING / cam.zoom;

  // snap to power-of-2 steps: ..., 0.5, 1, 2, 4, 8, ...
  const log2Raw = Math.log2(rawStep);
  const coarseStep = Math.pow(2, Math.ceil(log2Raw));
  const fineStep = coarseStep / 2;

  // t: 0 at coarse boundary (fine dots invisible), 1 at fine boundary (fully visible)
  const t = (coarseStep - rawStep) / fineStep;

  // dot size scales slightly with zoom so they don't vanish or bloat
  const r = DOT_RADIUS / cam.zoom;

  // draw fine grid dots at faded opacity (coarse dots drawn on top will cover them)
  if (t > 0.01) {
    ctx.fillStyle = `rgba(208,208,208,${t})`;
    const startX = Math.floor(tl.x / fineStep) * fineStep;
    const startY = Math.floor(tl.y / fineStep) * fineStep;
    for (let x = startX; x <= br.x; x += fineStep) {
      for (let y = startY; y <= br.y; y += fineStep) {
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  // draw coarse grid at full opacity on top
  ctx.fillStyle = DOT_COLOR;
  const coarseStartX = Math.floor(tl.x / coarseStep) * coarseStep;
  const coarseStartY = Math.floor(tl.y / coarseStep) * coarseStep;
  for (let x = coarseStartX; x <= br.x; x += coarseStep) {
    for (let y = coarseStartY; y <= br.y; y += coarseStep) {
      ctx.beginPath();
      ctx.arc(x, y, r, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

// --- rendering ---

export function drawGraph(
  ctx: CanvasRenderingContext2D,
  cam: Camera,
  nodes: GraphNode[],
  edges: Edge[],
  highlightIdx: number | null,
) {
  const invZoom = 1 / cam.zoom;

  // edges
  ctx.strokeStyle = "#555";
  ctx.lineWidth = invZoom * 4;
  for (const e of edges) {
    const a = nodes[e.from];
    const b = nodes[e.to];
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.stroke();
  }

  // nodes
  ctx.font = NODE_FONT;
  ctx.lineWidth = 2;
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i];
    const label = `${n.ast.type}: ${n.ast.text}`;
    const isHighlight = i === highlightIdx;

    const rx = n.x - n.w / 2;
    const ry = n.y - n.h / 2;

    ctx.fillStyle = isHighlight ? "#e8f0fe" : "#fff";
    ctx.fillRect(rx, ry, n.w, n.h);
    ctx.strokeStyle = isHighlight ? NODE_HIGHLIGHT : "#000";
    ctx.strokeRect(rx, ry, n.w, n.h);

    ctx.fillStyle = "#000";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(label, n.x, n.y);
  }
}

// --- hit testing ---

export function hitTest(
  nodes: GraphNode[],
  wx: number,
  wy: number,
): number | null {
  for (let i = nodes.length - 1; i >= 0; i--) {
    const n = nodes[i];
    if (
      wx >= n.x - n.w / 2 &&
      wx <= n.x + n.w / 2 &&
      wy >= n.y - n.h / 2 &&
      wy <= n.y + n.h / 2
    ) {
      return i;
    }
  }
  return null;
}

// --- find deepest AST node containing a source position ---

export function findNodeAtPos(nodes: GraphNode[], pos: number): number | null {
  let bestIdx: number | null = null;
  let bestSize = Infinity;
  for (let i = 0; i < nodes.length; i++) {
    const ast = nodes[i].ast;
    if (pos >= ast.spanStart && pos < ast.spanEnd) {
      const size = ast.spanEnd - ast.spanStart;
      if (size < bestSize) {
        bestSize = size;
        bestIdx = i;
      }
    }
  }
  return bestIdx;
}
