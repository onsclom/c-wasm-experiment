import type { ASTNode } from "./parser";
import type { Camera } from "./camera";

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
const DAMPING = 0.85;
const MAX_SPEED = 800;

const NODE_H = 24;
const NODE_PAD_X = 12;
const NODE_FONT = "13px monospace";
const NODE_COLOR = "#335";
const NODE_HIGHLIGHT = "#06c";
const EDGE_COLOR = "#aaa";
const TEXT_COLOR = "#fff";

// --- measure node widths (needs a canvas ctx) ---

export function measureNodes(ctx: CanvasRenderingContext2D, nodes: GraphNode[]) {
  ctx.font = NODE_FONT;
  for (const n of nodes) {
    const label = `${n.ast.type}: ${n.ast.text}`;
    n.w = ctx.measureText(label).width + NODE_PAD_X * 2;
  }
}

// --- build flat arrays from AST ---

export function buildGraph(root: ASTNode): { nodes: GraphNode[]; edges: Edge[] } {
  const nodes: GraphNode[] = [];
  const edges: Edge[] = [];

  function visit(ast: ASTNode, depth: number, sibIdx: number) {
    const idx = nodes.length;
    nodes.push({
      ast,
      x: sibIdx * 120 + Math.random() * 10,
      y: depth * 100 + Math.random() * 10,
      vx: 0,
      vy: 0,
      w: 100, // placeholder until measured
      h: NODE_H,
    });
    for (let i = 0; i < ast.children.length; i++) {
      const childIdx = nodes.length;
      visit(ast.children[i], depth + 1, i);
      edges.push({ from: idx, to: childIdx });
    }
  }
  visit(root, 0, 0);
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
}

// --- pre-settle: run simulation ahead of time, then fit camera ---

export function settleAndFitCamera(
  nodes: GraphNode[],
  edges: Edge[],
  cam: Camera,
  canvasW: number,
  canvasH: number,
) {
  // run ~2 seconds of simulation at 60fps
  for (let i = 0; i < 120; i++) {
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

// --- rendering ---

export function drawGraph(
  ctx: CanvasRenderingContext2D,
  nodes: GraphNode[],
  edges: Edge[],
  highlightIdx: number | null,
) {
  // edges
  ctx.strokeStyle = EDGE_COLOR;
  ctx.lineWidth = 1;
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
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i];
    const label = `${n.ast.type}: ${n.ast.text}`;
    const isHighlight = i === highlightIdx;

    ctx.fillStyle = isHighlight ? NODE_HIGHLIGHT : NODE_COLOR;
    const rx = n.x - n.w / 2;
    const ry = n.y - n.h / 2;
    ctx.beginPath();
    ctx.roundRect(rx, ry, n.w, n.h, 4);
    ctx.fill();

    ctx.fillStyle = TEXT_COLOR;
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

export function findNodeAtPos(
  nodes: GraphNode[],
  pos: number,
): number | null {
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
