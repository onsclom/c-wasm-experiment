interface WasmExports {
  memory: WebAssembly.Memory;
  get_input_buffer(): number;
  get_input_buffer_size(): number;
  parse_program(len: number): number;
  result_ok(): number;
  result_error_msg_ptr(): number;
  result_error_msg_len(): number;
  result_error_pos(): number;
  result_root(): number;
  node_type(ptr: number): number;
  node_type_name_ptr(ptr: number): number;
  node_type_name_len(ptr: number): number;
  node_first_child(ptr: number): number;
  node_next_sibling(ptr: number): number;
  node_token_start(ptr: number): number;
  node_token_end(ptr: number): number;
  node_span_start(ptr: number): number;
  node_span_end(ptr: number): number;
}

export interface ASTNode {
  type: string;
  text: string;
  spanStart: number;
  spanEnd: number;
  children: ASTNode[];
}

export interface ParseError {
  message: string;
  pos: number;
}

export type ParseResult =
  | { ok: true; root: ASTNode }
  | { ok: false; error: ParseError };

import wasmUrl from "./parser.wasm" with { type: "file" };

const { instance } = await WebAssembly.instantiateStreaming(fetch(wasmUrl));
const wasm = instance.exports as unknown as WasmExports;

export function parseCode(source: string): ParseResult {
  const encoder = new TextEncoder();
  const encoded = encoder.encode(source);

  const bufPtr = wasm.get_input_buffer();
  const bufSize = wasm.get_input_buffer_size();
  if (encoded.length > bufSize) {
    return {
      ok: false,
      error: {
        message: `Source too large (${encoded.length} > ${bufSize})`,
        pos: 0,
      },
    };
  }

  const mem = new Uint8Array(wasm.memory.buffer);
  mem.set(encoded, bufPtr);

  wasm.parse_program(encoded.length);

  if (!wasm.result_ok()) {
    const errPtr = wasm.result_error_msg_ptr();
    const errLen = wasm.result_error_msg_len();
    const errBytes = mem.slice(errPtr, errPtr + errLen);
    const message = new TextDecoder().decode(errBytes);
    return { ok: false, error: { message, pos: wasm.result_error_pos() } };
  }

  const rootPtr = wasm.result_root();
  const root = walkNode(rootPtr, source);
  return { ok: true, root };
}

const decoder = new TextDecoder();

function walkNode(ptr: number, source: string): ASTNode {
  const mem = new Uint8Array(wasm.memory.buffer);
  const namePtr = wasm.node_type_name_ptr(ptr);
  const nameLen = wasm.node_type_name_len(ptr);
  const type = decoder.decode(mem.slice(namePtr, namePtr + nameLen));

  const tokStart = wasm.node_token_start(ptr);
  const tokEnd = wasm.node_token_end(ptr);
  const text = source.slice(tokStart, tokEnd);

  const children: ASTNode[] = [];
  let childPtr = wasm.node_first_child(ptr);
  while (childPtr) {
    children.push(walkNode(childPtr, source));
    childPtr = wasm.node_next_sibling(childPtr);
  }

  const spanStart = wasm.node_span_start(ptr);
  const spanEnd = wasm.node_span_end(ptr);

  return { type, text, spanStart, spanEnd, children };
}
