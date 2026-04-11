#pragma once

#include "tokenize.h"

typedef struct {
  size_t start;
  size_t end;
} Span;

typedef enum {
  NODE_INT_LITERAL,
  NODE_FLOAT_LITERAL,
  NODE_STRING_LITERAL,
  NODE_CHAR_LITERAL,
  NODE_IDENTIFIER,

  NODE_BINARY_EXPR,  // left op right
  NODE_UNARY_EXPR,   // op operand
  NODE_CALL_EXPR,    // func(args)

  NODE_EXPR_STMT,    // expr ;
  NODE_RETURN_STMT,  // return expr ;
  NODE_VAR_DECL,     // type name = expr ;

  NODE_PARAM,        // type name
  NODE_FUNC_DEF,     // type name(params) { body }
  NODE_PROGRAM,      // list of top-level declarations
} ASTNodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
  ASTNodeType type;
  ASTNode *first_child;
  ASTNode *last_child;
  ASTNode *next_sibling;

  // which token this node came from (for operators, keywords, etc.)
  Token token;

  // source span covering this entire node
  Span span;
};

typedef struct {
  ASTNode *root;

  bool ok;
  s8 error_message;
  size_t error_pos;
} ParseResult;

ParseResult parse(Arena *arena, Token *tokens, size_t token_count);
