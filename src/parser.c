#include "parser.h"

static const s8 node_type_names[NODE_COUNT] = {
    [NODE_INT_LITERAL] = s8_lit("IntLiteral"),
    [NODE_FLOAT_LITERAL] = s8_lit("FloatLiteral"),
    [NODE_STRING_LITERAL] = s8_lit("StringLiteral"),
    [NODE_CHAR_LITERAL] = s8_lit("CharLiteral"),
    [NODE_IDENTIFIER] = s8_lit("Identifier"),
    [NODE_BINARY_EXPR] = s8_lit("BinaryExpr"),
    [NODE_UNARY_EXPR] = s8_lit("UnaryExpr"),
    [NODE_CALL_EXPR] = s8_lit("CallExpr"),
    [NODE_EXPR_STMT] = s8_lit("ExprStmt"),
    [NODE_RETURN_STMT] = s8_lit("ReturnStmt"),
    [NODE_VAR_DECL] = s8_lit("VarDecl"),
    [NODE_PARAM] = s8_lit("Param"),
    [NODE_FUNC_DEF] = s8_lit("FuncDef"),
    [NODE_PROGRAM] = s8_lit("Program"),
};

s8 node_type_name(ASTNodeType type) {
  if (type >= 0 && type < NODE_COUNT && node_type_names[type].length > 0)
    return node_type_names[type];
  return s8_lit("Unknown");
}

typedef struct {
  Arena *arena;
  Token *tokens;
  size_t count;
  size_t pos;

  bool had_error;
  s8 error_message;
  size_t error_pos;
} Parser;

static bool parser_at_end(Parser *p) {
  return p->had_error || p->pos >= p->count;
}

static Token parser_peek(Parser *p) {
  if (parser_at_end(p))
    return (Token){0};
  return p->tokens[p->pos];
}

static Token parser_advance(Parser *p) {
  if (parser_at_end(p))
    return (Token){0};
  return p->tokens[p->pos++];
}

static bool parser_check(Parser *p, TokenType type) {
  return !parser_at_end(p) && parser_peek(p).type == type;
}

static bool parser_match(Parser *p, TokenType type) {
  if (parser_check(p, type)) {
    parser_advance(p);
    return true;
  }
  return false;
}

static void parser_error(Parser *p, s8 msg, size_t pos) {
  if (!p->had_error) {
    p->had_error = true;
    p->error_message = msg;
    p->error_pos = pos;
  }
}

static Token parser_expect(Parser *p, TokenType type, s8 msg) {
  if (parser_check(p, type))
    return parser_advance(p);
  size_t pos = parser_at_end(p) ? p->tokens[p->count - 1].span.end
                                : parser_peek(p).span.start;
  parser_error(p, msg, pos);
  return (Token){0};
}

static ASTNode *make_node(Parser *p, ASTNodeType type, Token token) {
  ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
  node->type = type;
  node->first_child = NULL;
  node->last_child = NULL;
  node->next_sibling = NULL;
  node->token = token;
  node->span = token.span;
  return node;
}

static void add_child(ASTNode *parent, ASTNode *child) {
  if (!parent->first_child) {
    parent->first_child = child;
  } else {
    parent->last_child->next_sibling = child;
  }
  parent->last_child = child;
}

static i32 prefix_precedence(TokenType type) {
  switch (type) {
  case TOKEN_BANG:
  case TOKEN_MINUS:
    return 90;
  default:
    return -1;
  }
}

static i32 infix_precedence(TokenType type) {
  switch (type) {
  case TOKEN_OR_OR:
    return 10;
  case TOKEN_AND_AND:
    return 20;
  case TOKEN_EQ_EQ:
  case TOKEN_BANG_EQ:
    return 30;
  case TOKEN_LT:
  case TOKEN_LT_EQ:
  case TOKEN_GT:
  case TOKEN_GT_EQ:
    return 40;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
    return 50;
  case TOKEN_STAR:
  case TOKEN_DIVIDE:
  case TOKEN_PERCENT:
    return 60;
  default:
    return -1;
  }
}

static ASTNode *parse_expr(Parser *p, i32 min_prec);

static ASTNode *parse_prefix_or_atom(Parser *p) {
  Token tok = parser_peek(p);

  i32 prec = prefix_precedence(tok.type);
  if (prec >= 0) {
    parser_advance(p);
    ASTNode *operand = parse_expr(p, prec);
    if (!operand)
      return NULL;
    ASTNode *node = make_node(p, NODE_UNARY_EXPR, tok);
    node->span.end = operand->span.end;
    add_child(node, operand);
    return node;
  }

  switch (tok.type) {
  case TOKEN_INT_LITERAL:
    parser_advance(p);
    return make_node(p, NODE_INT_LITERAL, tok);
  case TOKEN_FLOAT_LITERAL:
    parser_advance(p);
    return make_node(p, NODE_FLOAT_LITERAL, tok);
  case TOKEN_STRING_LITERAL:
    parser_advance(p);
    return make_node(p, NODE_STRING_LITERAL, tok);
  case TOKEN_CHAR_LITERAL:
    parser_advance(p);
    return make_node(p, NODE_CHAR_LITERAL, tok);

  case TOKEN_IDENTIFIER: {
    parser_advance(p);
    if (parser_check(p, TOKEN_LPAREN)) {
      parser_advance(p);
      ASTNode *call = make_node(p, NODE_CALL_EXPR, tok);
      if (!parser_check(p, TOKEN_RPAREN)) {
        ASTNode *arg = parse_expr(p, 0);
        if (arg)
          add_child(call, arg);
        while (parser_match(p, TOKEN_COMMA)) {
          arg = parse_expr(p, 0);
          if (arg)
            add_child(call, arg);
        }
      }
      Token rparen = parser_expect(p, TOKEN_RPAREN,
                                   s8_lit("Expected ')' after arguments"));
      call->span.end = rparen.span.end;
      return call;
    }
    return make_node(p, NODE_IDENTIFIER, tok);
  }

  case TOKEN_LPAREN: {
    parser_advance(p);
    ASTNode *inner = parse_expr(p, 0);
    if (!inner)
      return NULL;
    parser_expect(p, TOKEN_RPAREN, s8_lit("Expected ')' after expression"));
    return inner;
  }

  default:
    parser_error(p, s8_lit("Expected expression"), tok.span.start);
    return NULL;
  }
}

static ASTNode *parse_expr(Parser *p, i32 min_prec) {
  ASTNode *left = parse_prefix_or_atom(p);
  if (!left)
    return NULL;

  while (!parser_at_end(p)) {
    Token op = parser_peek(p);
    i32 prec = infix_precedence(op.type);
    if (prec < 0 || prec < min_prec)
      break;

    parser_advance(p);
    ASTNode *right = parse_expr(p, prec + 1);
    if (!right)
      return NULL;

    ASTNode *bin = make_node(p, NODE_BINARY_EXPR, op);
    bin->span = (Span){left->span.start, right->span.end};
    add_child(bin, left);
    add_child(bin, right);
    left = bin;
  }

  return left;
}

static bool is_type_keyword(TokenType type) {
  return type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_FLOAT ||
         type == TOKEN_KEYWORD_CHAR || type == TOKEN_KEYWORD_VOID;
}

static ASTNode *parse_statement(Parser *p) {
  Token tok = parser_peek(p);

  if (tok.type == TOKEN_KEYWORD_RETURN) {
    parser_advance(p);
    ASTNode *node = make_node(p, NODE_RETURN_STMT, tok);
    if (!parser_check(p, TOKEN_SEMICOLON)) {
      ASTNode *expr = parse_expr(p, 0);
      if (expr)
        add_child(node, expr);
    }
    Token semi =
        parser_expect(p, TOKEN_SEMICOLON, s8_lit("Expected ';' after return"));
    node->span.end = semi.span.end;
    return node;
  }

  if (is_type_keyword(tok.type)) {
    size_t saved_pos = p->pos;
    Token type_tok = parser_advance(p);

    if (parser_check(p, TOKEN_IDENTIFIER)) {
      Token name_tok = parser_advance(p);

      if (parser_check(p, TOKEN_EQ) || parser_check(p, TOKEN_SEMICOLON)) {
        ASTNode *decl = make_node(p, NODE_VAR_DECL, type_tok);
        ASTNode *name_node = make_node(p, NODE_IDENTIFIER, name_tok);
        add_child(decl, name_node);

        if (parser_match(p, TOKEN_EQ)) {
          ASTNode *init = parse_expr(p, 0);
          if (init)
            add_child(decl, init);
        }
        Token semi = parser_expect(p, TOKEN_SEMICOLON,
                                   s8_lit("Expected ';' after declaration"));
        decl->span.end = semi.span.end;
        return decl;
      }
    }

    p->pos = saved_pos;
  }

  ASTNode *expr = parse_expr(p, 0);
  if (!expr)
    return NULL;
  ASTNode *stmt = make_node(p, NODE_EXPR_STMT, expr->token);
  stmt->span.start = expr->span.start;
  add_child(stmt, expr);
  Token semi = parser_expect(p, TOKEN_SEMICOLON,
                             s8_lit("Expected ';' after expression"));
  stmt->span.end = semi.span.end;
  return stmt;
}

static ASTNode *parse_func_def(Parser *p) {
  Token type_tok = parser_peek(p);
  if (!is_type_keyword(type_tok.type)) {
    parser_error(p, s8_lit("Expected type keyword"), type_tok.span.start);
    return NULL;
  }
  parser_advance(p);

  Token name_tok =
      parser_expect(p, TOKEN_IDENTIFIER, s8_lit("Expected function name"));

  ASTNode *func = make_node(p, NODE_FUNC_DEF, type_tok);
  add_child(func, make_node(p, NODE_IDENTIFIER, name_tok));

  parser_expect(p, TOKEN_LPAREN, s8_lit("Expected '(' after function name"));

  if (!parser_check(p, TOKEN_RPAREN)) {
    if (parser_check(p, TOKEN_KEYWORD_VOID) && p->pos + 1 < p->count &&
        p->tokens[p->pos + 1].type == TOKEN_RPAREN) {
      parser_advance(p);
    } else {
      do {
        Token param_type = parser_peek(p);
        if (!is_type_keyword(param_type.type)) {
          parser_error(p, s8_lit("Expected parameter type"),
                       param_type.span.start);
          return NULL;
        }
        parser_advance(p);
        Token param_name = parser_expect(p, TOKEN_IDENTIFIER,
                                         s8_lit("Expected parameter name"));
        ASTNode *param = make_node(p, NODE_PARAM, param_type);
        ASTNode *pname = make_node(p, NODE_IDENTIFIER, param_name);
        add_child(param, pname);
        param->span.end = param_name.span.end;
        add_child(func, param);
      } while (parser_match(p, TOKEN_COMMA));
    }
  }

  parser_expect(p, TOKEN_RPAREN, s8_lit("Expected ')' after parameters"));
  parser_expect(p, TOKEN_LBRACE, s8_lit("Expected '{' before function body"));

  while (!parser_at_end(p) && !parser_check(p, TOKEN_RBRACE)) {
    ASTNode *stmt = parse_statement(p);
    if (stmt)
      add_child(func, stmt);
  }

  Token rbrace = parser_expect(p, TOKEN_RBRACE,
                               s8_lit("Expected '}' after function body"));
  func->span.end = rbrace.span.end;
  return func;
}

ParseResult parse(Arena *arena, Token *tokens, size_t token_count) {
  Parser p = {
      .arena = arena,
      .tokens = tokens,
      .count = token_count,
      .pos = 0,
      .had_error = false,
  };

  Token first_tok = token_count > 0 ? tokens[0] : (Token){0};
  ASTNode *program = make_node(&p, NODE_PROGRAM, first_tok);

  while (!parser_at_end(&p)) {
    ASTNode *func = parse_func_def(&p);
    if (func)
      add_child(program, func);
  }

  if (program->last_child)
    program->span.end = program->last_child->span.end;

  if (p.had_error) {
    return (ParseResult){
        .root = NULL,
        .ok = false,
        .error_message = p.error_message,
        .error_pos = p.error_pos,
    };
  }

  return (ParseResult){
      .root = program,
      .ok = true,
  };
}
