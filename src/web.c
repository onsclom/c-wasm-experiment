// WASM entry point for the parser.
// Compile with: clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all
//
// JS calls parse_program(ptr, len) then reads results via accessor functions.

#include "base/base_inc.h"
#include "base/base_inc.c"
#include "tokenize.h"
#include "tokenize.c"
#include "parser.h"
#include "parser.c"

// 1 MB arena
#define ARENA_SIZE (1024 * 1024)
static u8 arena_buf[ARENA_SIZE] __attribute__((aligned(8)));
static Arena arena = {.base = arena_buf, .cap = ARENA_SIZE, .len = 0};

// stash last parse result
static ParseResult last_result;

// --- exposed to JS ---

// Returns a pointer into wasm memory where JS should write the source string.
// We use the end of the arena buffer as a scratch area.
#define INPUT_BUF_SIZE (64 * 1024)
static u8 input_buf[INPUT_BUF_SIZE];

u8 *get_input_buffer(void) { return input_buf; }

size_t get_input_buffer_size(void) { return INPUT_BUF_SIZE; }

// Tokenize + parse. Returns 1 on success, 0 on error.
i32 parse_program(size_t src_len) {
  arena_reset(&arena);

  s8 source = {.data = input_buf, .length = src_len};

  TokenizeResult tok_result = tokenize(&arena, source);
  if (!tok_result.ok) {
    last_result = (ParseResult){
        .root = NULL,
        .ok = false,
        .error_message = tok_result.error_message,
        .error_pos = tok_result.error_pos,
    };
    return 0;
  }

  last_result = parse(&arena, tok_result.tokens, tok_result.count);
  return last_result.ok ? 1 : 0;
}

// --- result accessors ---

i32 result_ok(void) { return last_result.ok ? 1 : 0; }

u8 *result_error_msg_ptr(void) { return last_result.error_message.data; }

size_t result_error_msg_len(void) { return last_result.error_message.length; }

size_t result_error_pos(void) { return last_result.error_pos; }

// --- AST node accessors ---
// JS passes ASTNode pointers (as u32 offsets in wasm memory).

size_t result_root(void) { return (size_t)last_result.root; }

i32 node_type(ASTNode *node) { return (i32)node->type; }

u8 *node_type_name_ptr(ASTNode *node) { return node_type_name(node->type).data; }

size_t node_type_name_len(ASTNode *node) { return node_type_name(node->type).length; }

size_t node_first_child(ASTNode *node) { return (size_t)node->first_child; }

size_t node_next_sibling(ASTNode *node) { return (size_t)node->next_sibling; }

size_t node_token_start(ASTNode *node) { return node->token.span.start; }

size_t node_token_end(ASTNode *node) { return node->token.span.end; }

size_t node_span_start(ASTNode *node) { return node->span.start; }

size_t node_span_end(ASTNode *node) { return node->span.end; }
