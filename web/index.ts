import { startLoop } from "./canvas";
import { parseCode, type ASTNode } from "./parser";
import {
  buildGraph,
  simulate,
  drawGraph,
  drawDotGrid,
  hitTest,
  findNodeAtPos,
  measureNodes,
  settleAndFitCamera,
  type GraphNode,
} from "./graph";
import { createCamera, applyCamera, setupCameraControls } from "./camera";

// --- build DOM ---

const title = document.createElement("strong");
title.textContent = "parser ast visualizer";

const actionBtn = document.createElement("button");
actionBtn.textContent = "parse";
actionBtn.style.cssText = "padding:6px 20px; font-size:15px;";

const header = document.createElement("div");
header.style.cssText = "display:flex; align-items:center; gap:10px; padding:8px 12px; flex-shrink:0;";
header.append(title, actionBtn);

const main = document.createElement("div");
main.style.cssText = "flex:1; display:flex; overflow:hidden;";

// editor view
const editorView = document.createElement("div");
editorView.style.cssText = "flex:1; display:flex; flex-direction:column;";

const codeInput = document.createElement("textarea");
codeInput.spellcheck = false;
codeInput.placeholder = "Write some C code...";
codeInput.value = `int square(int x) {
    return x * x;
}

int add(int a, int b) {
    return a + b;
}

int main() {
    int a = square(3);
    int b = add(a, 10);
    int c = a * b + 2;
    return c % 5 == 0 || c > 100;
}`;
codeInput.style.cssText =
  "flex:1; font-family:monospace; font-size:14px; tab-size:4; resize:none; padding:8px 12px;";

editorView.append(header, codeInput);

// split view
const splitView = document.createElement("div");
splitView.style.cssText = "flex:1; display:none;";

const codePanel = document.createElement("div");
codePanel.style.cssText =
  "width:50%; display:flex; flex-direction:column; cursor:default;";

const codeDisplay = document.createElement("pre");
codeDisplay.style.cssText =
  "font-family:monospace; font-size:14px; white-space:pre; line-height:1.5; cursor:crosshair;";
const codeScroll = document.createElement("div");
codeScroll.style.cssText = "flex:1; overflow:auto; padding:0 12px 12px;";
codeScroll.append(codeDisplay);
codePanel.append(codeScroll);

const divider = document.createElement("div");
divider.style.cssText =
  "width:16px; margin-left:-8px; margin-right:-8px; cursor:col-resize; flex-shrink:0; display:flex; justify-content:center; position:relative; z-index:1;";
const dividerLine = document.createElement("div");
dividerLine.style.cssText = "width:4px; height:100%; background:#ddd;";
divider.append(dividerLine);
divider.addEventListener("mouseenter", () => (dividerLine.style.background = "#aaa"));
divider.addEventListener("mouseleave", () => {
  if (!isDraggingDivider) dividerLine.style.background = "#ddd";
});

const canvasPanel = document.createElement("div");
canvasPanel.style.cssText = "flex:1; min-width:0;";

const canvas = document.createElement("canvas");
canvas.style.cssText = "width:100%; height:100%; display:block;";
canvasPanel.append(canvas);

splitView.append(codePanel, divider, canvasPanel);

// --- divider drag ---

let isDraggingDivider = false;

divider.addEventListener("mousedown", (e) => {
  isDraggingDivider = true;
  dividerLine.style.background = "#aaa";
  e.preventDefault();
});

window.addEventListener("mousemove", (e) => {
  if (!isDraggingDivider) return;
  const rect = splitView.getBoundingClientRect();
  const pct = ((e.clientX - rect.left) / rect.width) * 100;
  const clamped = Math.max(10, Math.min(90, pct));
  codePanel.style.width = clamped + "%";
});

window.addEventListener("mouseup", () => {
  if (!isDraggingDivider) return;
  isDraggingDivider = false;
  dividerLine.style.background = "#ddd";
});
main.append(editorView, splitView);
document.body.append(main);

// --- state ---

let inParseView = false;
let graphNodes: GraphNode[] = [];
let graphEdges: { from: number; to: number }[] = [];
let highlightIdx: number | null = null;
let dragIdx: number | null = null;
let sourceCode = "";

const cam = createCamera();

// --- code panel: render source as per-character spans ---

function renderCodePanel(source: string, error?: { pos: number; message: string }) {
  codeDisplay.innerHTML = "";
  for (let i = 0; i < source.length; i++) {
    const span = document.createElement("span");
    span.textContent = source[i];
    span.dataset.pos = String(i);
    codeDisplay.appendChild(span);
  }

  if (error) {
    // find the line containing the error and highlight from error pos to end of that line
    const lineStart = source.lastIndexOf("\n", error.pos - 1) + 1;
    let lineEnd = source.indexOf("\n", error.pos);
    if (lineEnd === -1) lineEnd = source.length;

    // if error is at end of line / whitespace, highlight at least one char back
    const errStart = error.pos;
    const errEnd = Math.max(error.pos + 1, lineEnd);

    const spans = codeDisplay.children;
    for (let i = errStart; i < errEnd && i < spans.length; i++) {
      const el = spans[i] as HTMLElement;
      el.style.backgroundColor = "#fdd";
      el.style.textDecoration = "wavy underline red";
      el.style.textDecorationSkipInk = "none";
      el.style.textUnderlineOffset = "3px";
    }

    // insert inline error tooltip after the error region
    const tooltip = document.createElement("span");
    tooltip.textContent = `  ← ${error.message}`;
    tooltip.style.cssText =
      "color:#c33; font-size:12px; font-style:italic; pointer-events:none; user-select:none;";

    // insert after the last char of the error line
    const anchor = spans[Math.min(errEnd, spans.length) - 1];
    if (anchor) {
      anchor.after(tooltip);
      // scroll error into view
      (spans[errStart] as HTMLElement)?.scrollIntoView({ block: "center", behavior: "smooth" });
    }
  }
}

function highlightCodeRange(start: number, end: number) {
  const spans = codeDisplay.children;
  for (let i = 0; i < spans.length; i++) {
    const el = spans[i] as HTMLElement;
    if (i >= start && i < end) {
      el.style.backgroundColor = "#cde";
    } else {
      el.style.backgroundColor = "";
    }
  }
}

function clearCodeHighlight() {
  const spans = codeDisplay.children;
  for (let i = 0; i < spans.length; i++) {
    (spans[i] as HTMLElement).style.backgroundColor = "";
  }
}

// --- code panel hover -> highlight graph node ---

codeDisplay.addEventListener("mousemove", (e) => {
  const target = e.target as HTMLElement;
  if (target.dataset.pos != null) {
    const pos = parseInt(target.dataset.pos);
    highlightIdx = findNodeAtPos(graphNodes, pos);
    if (highlightIdx !== null) {
      const ast = graphNodes[highlightIdx].ast;
      highlightCodeRange(ast.spanStart, ast.spanEnd);
    } else {
      clearCodeHighlight();
    }
  }
});

codeDisplay.addEventListener("mouseleave", () => {
  highlightIdx = null;
  clearCodeHighlight();
});

// --- canvas mouse interaction ---

setupCameraControls(canvas, cam, {
  onMouseDown(wx, wy) {
    const idx = hitTest(graphNodes, wx, wy);
    if (idx !== null) {
      dragIdx = idx;
      return true;
    }
    return false; // not on a node — let camera pan
  },
  onMouseMove(wx, wy, sx, sy, e) {
    if (dragIdx !== null) {
      graphNodes[dragIdx].x = wx;
      graphNodes[dragIdx].y = wy;
      return;
    }
    // hover
    const rect = canvas.getBoundingClientRect();
    if (sx >= 0 && sy >= 0 && sx <= rect.width && sy <= rect.height) {
      const idx = hitTest(graphNodes, wx, wy);
      highlightIdx = idx;
      if (!e.shiftKey) {
        canvas.style.cursor = idx !== null ? "grab" : "crosshair";
      }
      if (idx !== null) {
        const ast = graphNodes[idx].ast;
        highlightCodeRange(ast.spanStart, ast.spanEnd);
      } else {
        clearCodeHighlight();
      }
    }
  },
  onMouseUp() {
    dragIdx = null;
  },
});

// --- tab handling ---

codeInput.addEventListener("keydown", (e) => {
  const start = codeInput.selectionStart;
  const end = codeInput.selectionEnd;
  const val = codeInput.value;

  if (e.key === "Tab" && !e.shiftKey) {
    e.preventDefault();
    codeInput.value = val.substring(0, start) + "    " + val.substring(end);
    codeInput.selectionStart = codeInput.selectionEnd = start + 4;
  } else if (e.key === "Tab" && e.shiftKey) {
    e.preventDefault();
    const lineStart = val.lastIndexOf("\n", start - 1) + 1;
    const linePrefix = val.substring(lineStart, start);
    const spaces = linePrefix.match(/^ {1,4}/)?.[0].length ?? 0;
    if (spaces > 0) {
      codeInput.value =
        val.substring(0, lineStart) + val.substring(lineStart + spaces);
      codeInput.selectionStart = codeInput.selectionEnd = start - spaces;
    }
  } else if (e.key === "Backspace" && start === end) {
    const before = val.substring(0, start);
    if (before.endsWith("    ")) {
      e.preventDefault();
      codeInput.value = val.substring(0, start - 4) + val.substring(end);
      codeInput.selectionStart = codeInput.selectionEnd = start - 4;
    }
  }
});

// --- parse button ---

actionBtn.addEventListener("click", () => {
  if (inParseView) {
    editorView.style.display = "flex";
    splitView.style.display = "none";
    editorView.prepend(header);
    actionBtn.textContent = "parse";
    inParseView = false;
    return;
  }

  sourceCode = codeInput.value;
  const result = parseCode(sourceCode);

  editorView.style.display = "none";
  splitView.style.display = "flex";
  codePanel.prepend(header);
  actionBtn.textContent = "edit";
  inParseView = true;

  if (!result.ok) {
    renderCodePanel(sourceCode, result.error);
    graphNodes = [];
    graphEdges = [];

    startLoop(canvas, (ctx) => {
      const w = canvas.width / devicePixelRatio;
      const h = canvas.height / devicePixelRatio;
      ctx.fillStyle = "#fff";
      ctx.fillRect(0, 0, w, h);
    });
    return;
  }

  renderCodePanel(sourceCode);

  const graph = buildGraph(result.root);
  graphNodes = graph.nodes;
  graphEdges = graph.edges;
  highlightIdx = null;
  dragIdx = null;

  // measure node widths, pre-settle, and fit camera
  const tempCtx = canvas.getContext("2d")!;
  measureNodes(tempCtx, graphNodes);
  const rect = canvas.getBoundingClientRect();
  settleAndFitCamera(graphNodes, graphEdges, cam, rect.width, rect.height);

  startLoop(canvas, (ctx, dt) => {
    const w = canvas.width / devicePixelRatio;
    const h = canvas.height / devicePixelRatio;

    simulate(graphNodes, graphEdges, dt, dragIdx);

    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, w, h);

    ctx.save();
    applyCamera(ctx, cam, w, h);
    drawDotGrid(ctx, cam, w, h);
    drawGraph(ctx, cam, graphNodes, graphEdges, highlightIdx);
    ctx.restore();
  });
});
