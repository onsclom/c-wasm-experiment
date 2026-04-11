import { startLoop } from "./canvas";
import { parseCode, type ASTNode } from "./parser";
import {
  buildGraph,
  simulate,
  drawGraph,
  hitTest,
  findNodeAtPos,
  measureNodes,
  settleAndFitCamera,
  type GraphNode,
} from "./graph";
import { createCamera, applyCamera, setupCameraControls } from "./camera";

// --- build DOM ---

const toolbar = document.createElement("div");
toolbar.style.cssText =
  "display:flex; align-items:center; gap:10px; padding:8px 12px;";

const title = document.createElement("strong");
title.textContent = "parser visualizer";

const actionBtn = document.createElement("button");
actionBtn.textContent = "parse";
actionBtn.style.cssText = "padding:6px 20px; font-size:15px;";

const errorDisplay = document.createElement("span");
errorDisplay.style.cssText = "color:red; font-size:13px;";

toolbar.append(title, actionBtn, errorDisplay);

const main = document.createElement("div");
main.style.cssText = "flex:1; display:flex; overflow:hidden;";

// editor view
const editorView = document.createElement("div");
editorView.style.cssText = "flex:1; display:flex; padding:8px 12px 12px;";

const codeInput = document.createElement("textarea");
codeInput.spellcheck = false;
codeInput.placeholder = "Write some C code...";
codeInput.value = `int main() {
    int x = 5;
    int y = 10;
    return x + y;
}`;
codeInput.style.cssText =
  "flex:1; font-family:monospace; font-size:14px; tab-size:4; resize:none; padding:8px;";

editorView.append(codeInput);

// split view
const splitView = document.createElement("div");
splitView.style.cssText = "flex:1; display:none;";

const codePanel = document.createElement("div");
codePanel.style.cssText =
  "width:50%; overflow:auto; border-right:1px solid; padding:8px 12px; cursor:default;";

const codeDisplay = document.createElement("pre");
codeDisplay.style.cssText =
  "font-family:monospace; font-size:14px; white-space:pre; line-height:1.5;";
codePanel.append(codeDisplay);

const canvasPanel = document.createElement("div");
canvasPanel.style.cssText = "flex:1;";

const canvas = document.createElement("canvas");
canvas.style.cssText = "width:100%; height:100%; display:block;";
canvasPanel.append(canvas);

splitView.append(codePanel, canvasPanel);
main.append(editorView, splitView);
document.body.append(toolbar, main);

// --- state ---

let inParseView = false;
let graphNodes: GraphNode[] = [];
let graphEdges: { from: number; to: number }[] = [];
let highlightIdx: number | null = null;
let dragIdx: number | null = null;
let sourceCode = "";

const cam = createCamera();

// --- code panel: render source as per-character spans ---

function renderCodePanel(source: string) {
  codeDisplay.innerHTML = "";
  for (let i = 0; i < source.length; i++) {
    const span = document.createElement("span");
    span.textContent = source[i];
    span.dataset.pos = String(i);
    codeDisplay.appendChild(span);
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
    }
  },
  onMouseMove(wx, wy, sx, sy) {
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
      canvas.style.cursor = idx !== null ? "grab" : "default";
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
    actionBtn.textContent = "parse";
    errorDisplay.textContent = "";
    inParseView = false;
    return;
  }

  sourceCode = codeInput.value;
  const result = parseCode(sourceCode);

  editorView.style.display = "none";
  splitView.style.display = "flex";
  actionBtn.textContent = "edit";
  inParseView = true;

  if (!result.ok) {
    renderCodePanel(sourceCode);
    errorDisplay.textContent = `Error at pos ${result.error.pos}: ${result.error.message}`;
    graphNodes = [];
    graphEdges = [];
    // highlight error position in code
    highlightCodeRange(result.error.pos, result.error.pos + 1);

    startLoop(canvas, (ctx) => {
      const w = canvas.width / devicePixelRatio;
      const h = canvas.height / devicePixelRatio;
      ctx.fillStyle = "#fff";
      ctx.fillRect(0, 0, w, h);
      ctx.fillStyle = "#c33";
      ctx.font = "16px system-ui";
      ctx.textAlign = "center";
      ctx.fillText(`Parse error: ${result.error.message}`, w / 2, h / 2);
    });
    return;
  }

  errorDisplay.textContent = "";
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
    drawGraph(ctx, graphNodes, graphEdges, highlightIdx);
    ctx.restore();
  });
});
