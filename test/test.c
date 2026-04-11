#include "../src/base/base_inc.c"
#include "../src/parser.c"
#include "../src/tokenize.c"

#include <stdio.h>
#include <stdlib.h>

// ===== test infrastructure =====

static u8 arena_mem[256 * 1024];
static Arena arena;

static i32 tests_run = 0;
static i32 tests_passed = 0;
static i32 tests_failed = 0;

// ===== string buffer for AST printing =====

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void buf_char(StrBuf *b, char c) {
  if (b->len < b->cap - 1)
    b->buf[b->len++] = c;
  b->buf[b->len] = '\0';
}

static void buf_str(StrBuf *b, const char *s) {
  while (*s)
    buf_char(b, *s++);
}

static void buf_indent(StrBuf *b, i32 depth) {
  for (i32 i = 0; i < depth * 2; i++)
    buf_char(b, ' ');
}

static void buf_token_text(StrBuf *b, Token tok, s8 source) {
  for (size_t i = tok.start; i < tok.end && i < source.length; i++)
    buf_char(b, (char)source.data[i]);
}

// ===== AST pretty printer =====

static const char *ast_node_name(ASTNodeType type) {
  switch (type) {
  case NODE_PROGRAM:
    return "Program";
  case NODE_FUNC_DEF:
    return "FuncDef";
  case NODE_PARAM:
    return "Param";
  case NODE_IDENTIFIER:
    return "Identifier";
  case NODE_INT_LITERAL:
    return "IntLiteral";
  case NODE_FLOAT_LITERAL:
    return "FloatLiteral";
  case NODE_STRING_LITERAL:
    return "StringLiteral";
  case NODE_CHAR_LITERAL:
    return "CharLiteral";
  case NODE_BINARY_EXPR:
    return "BinaryExpr";
  case NODE_UNARY_EXPR:
    return "UnaryExpr";
  case NODE_CALL_EXPR:
    return "CallExpr";
  case NODE_EXPR_STMT:
    return "ExprStmt";
  case NODE_RETURN_STMT:
    return "ReturnStmt";
  case NODE_VAR_DECL:
    return "VarDecl";
  }
  return "Unknown";
}

static bool ast_node_shows_token(ASTNodeType type) {
  switch (type) {
  case NODE_PROGRAM:
  case NODE_EXPR_STMT:
  case NODE_RETURN_STMT:
    return false;
  default:
    return true;
  }
}

static void ast_print_node(StrBuf *b, ASTNode *node, s8 source, i32 depth) {
  if (!node)
    return;

  buf_indent(b, depth);
  buf_str(b, ast_node_name(node->type));

  if (ast_node_shows_token(node->type)) {
    buf_str(b, " '");
    buf_token_text(b, node->token, source);
    buf_char(b, '\'');
  }

  buf_char(b, '\n');

  for (ASTNode *child = node->first_child; child; child = child->next_sibling)
    ast_print_node(b, child, source, depth + 1);
}

static char ast_buf[8192];

static const char *ast_print(ASTNode *root, s8 source) {
  StrBuf b = {.buf = ast_buf, .len = 0, .cap = sizeof(ast_buf)};
  ast_print_node(&b, root, source, 0);
  return ast_buf;
}

// ===== file reading =====

static s8 read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("        ERROR: could not open '%s'\n", path);
    return (s8){0};
  }
  fseek(f, 0, SEEK_END);
  size_t size = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);

  u8 *data = arena_alloc(&arena, size + 1);
  fread(data, 1, size, f);
  data[size] = '\0';
  fclose(f);

  return (s8){.data = data, .length = size};
}

// ===== line-by-line diff display =====

static void show_diff(const char *expected, const char *actual) {
  printf("        --- expected ---\n");
  const char *p = expected;
  i32 line = 1;
  while (*p) {
    printf("        %3d | ", line++);
    while (*p && *p != '\n') {
      putchar(*p);
      p++;
    }
    putchar('\n');
    if (*p == '\n')
      p++;
  }
  printf("        --- actual ---\n");
  p = actual;
  line = 1;
  while (*p) {
    printf("        %3d | ", line++);
    while (*p && *p != '\n') {
      putchar(*p);
      p++;
    }
    putchar('\n');
    if (*p == '\n')
      p++;
  }
}

// ===== string comparison =====

static bool str_eq(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b)
      return false;
    a++;
    b++;
  }
  return *a == *b;
}

// ===== test helpers =====

static void test_parse_file(const char *name, const char *path,
                            const char *expected_ast) {
  tests_run++;
  arena_reset(&arena);

  s8 source = read_file(path);
  if (!source.data) {
    tests_failed++;
    printf("  FAIL  %s (file read)\n", name);
    return;
  }

  TokenizeResult toks = tokenize(&arena, source);
  if (!toks.ok) {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        tokenize error: %.*s\n", (i32)toks.error_message.length,
           (char *)toks.error_message.data);
    return;
  }

  ParseResult pr = parse(&arena, toks.tokens, toks.count);
  if (!pr.ok) {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        parse error: %.*s\n", (i32)pr.error_message.length,
           (char *)pr.error_message.data);
    return;
  }

  const char *actual = ast_print(pr.root, source);
  if (str_eq(actual, expected_ast)) {
    tests_passed++;
    printf("  PASS  %s\n", name);
  } else {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    show_diff(expected_ast, actual);
  }
}

static void test_parse_error(const char *name, const char *path) {
  tests_run++;
  arena_reset(&arena);

  s8 source = read_file(path);
  if (!source.data) {
    tests_failed++;
    printf("  FAIL  %s (file read)\n", name);
    return;
  }

  TokenizeResult toks = tokenize(&arena, source);
  if (!toks.ok) {
    // tokenize error counts as expected failure
    tests_passed++;
    printf("  PASS  %s (tokenize error: %.*s)\n", name,
           (i32)toks.error_message.length, (char *)toks.error_message.data);
    return;
  }

  ParseResult pr = parse(&arena, toks.tokens, toks.count);
  if (!pr.ok) {
    tests_passed++;
    printf("  PASS  %s (parse error: %.*s)\n", name,
           (i32)pr.error_message.length, (char *)pr.error_message.data);
  } else {
    tests_failed++;
    printf("  FAIL  %s - expected error but parsed successfully\n", name);
    printf("        AST:\n%s", ast_print(pr.root, source));
  }
}

// ===== tokenizer test helpers (kept from original) =====

typedef struct {
  TokenType type;
  const char *text;
} Expected;

static void test_tokens(const char *name, const char *input,
                        const Expected *expected, size_t expected_count) {
  tests_run++;
  arena_reset(&arena);

  s8 source = {.data = (u8 *)input, .length = 0};
  while (input[source.length])
    source.length++;

  TokenizeResult result = tokenize(&arena, source);

  if (!result.ok) {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        tokenize error: %.*s\n", (i32)result.error_message.length,
           (char *)result.error_message.data);
    return;
  }

  bool pass = true;
  if (result.count != expected_count) {
    pass = false;
  } else {
    for (size_t i = 0; i < expected_count; i++) {
      Token *tok = &result.tokens[i];
      if (tok->type != expected[i].type) {
        pass = false;
        break;
      }
      size_t tok_len = tok->end - tok->start;
      const u8 *tok_text = source.data + tok->start;
      const char *exp_text = expected[i].text;
      size_t exp_len = 0;
      while (exp_text[exp_len])
        exp_len++;
      if (tok_len != exp_len) {
        pass = false;
        break;
      }
      for (size_t j = 0; j < tok_len; j++) {
        if (tok_text[j] != (u8)exp_text[j]) {
          pass = false;
          break;
        }
      }
    }
  }

  if (pass) {
    tests_passed++;
    printf("  PASS  %s\n", name);
  } else {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        expected %zu tokens, got %zu\n", expected_count,
           result.count);
    size_t max = result.count > expected_count ? result.count : expected_count;
    for (size_t i = 0; i < max; i++) {
      char got_buf[64] = "---";
      char got_text[64] = "";
      char exp_buf[64] = "---";
      char exp_text[64] = "";

      if (i < result.count) {
        Token *tok = &result.tokens[i];
        s8 tname = token_type_name(tok->type);
        snprintf(got_buf, sizeof(got_buf), "%.*s", (i32)tname.length,
                 (char *)tname.data);
        size_t tok_len = tok->end - tok->start;
        if (tok_len >= sizeof(got_text))
          tok_len = sizeof(got_text) - 1;
        for (size_t j = 0; j < tok_len; j++)
          got_text[j] = (char)source.data[tok->start + j];
        got_text[tok_len] = '\0';
      }
      if (i < expected_count) {
        s8 tname = token_type_name(expected[i].type);
        snprintf(exp_buf, sizeof(exp_buf), "%.*s", (i32)tname.length,
                 (char *)tname.data);
        snprintf(exp_text, sizeof(exp_text), "%s", expected[i].text);
      }

      bool match = i < result.count && i < expected_count &&
                   result.tokens[i].type == expected[i].type;
      const char *marker = match ? " " : "*";
      printf("      %s [%zu] got %-14s \"%s\"  expected %-14s \"%s\"\n", marker,
             i, got_buf, got_text, exp_buf, exp_text);
    }
  }
}

static void test_error(const char *name, const char *input,
                       const char *expected_msg) {
  tests_run++;
  arena_reset(&arena);

  s8 source = {.data = (u8 *)input, .length = 0};
  while (input[source.length])
    source.length++;

  TokenizeResult result = tokenize(&arena, source);

  if (result.ok) {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        expected error but tokenize succeeded with %zu tokens\n",
           result.count);
    return;
  }

  if (expected_msg) {
    s8 msg = result.error_message;
    size_t exp_len = 0;
    while (expected_msg[exp_len])
      exp_len++;
    bool match = msg.length == exp_len;
    if (match) {
      for (size_t i = 0; i < exp_len; i++) {
        if (msg.data[i] != (u8)expected_msg[i]) {
          match = false;
          break;
        }
      }
    }
    if (!match) {
      tests_failed++;
      printf("  FAIL  %s\n", name);
      printf("        expected error \"%s\", got \"%.*s\"\n", expected_msg,
             (i32)msg.length, (char *)msg.data);
      return;
    }
  }

  tests_passed++;
  printf("  PASS  %s\n", name);
}

// =====================================================================
//  main
// =====================================================================

int main(void) {
  arena = (Arena){.base = arena_mem, .cap = sizeof(arena_mem), .len = 0};

  // ===== TOKENIZER TESTS =====
  printf("===== TOKENIZER TESTS =====\n");

  printf("\n--- punctuation ---\n");
  {
    Expected e[] = {
        {TOKEN_LBRACE, "{"},   {TOKEN_SEMICOLON, ";"}, {TOKEN_LPAREN, "("},
        {TOKEN_LBRACKET, "["}, {TOKEN_RBRACKET, "]"},  {TOKEN_RBRACE, "}"},
        {TOKEN_COMMA, ","},
    };
    test_tokens("braces and brackets", "{;([]},", e, 7);
  }
  {
    Expected e[] = {{TOKEN_COLON, ":"}};
    test_tokens("colon", ":", e, 1);
  }

  printf("\n--- operators ---\n");
  {
    Expected e[] = {
        {TOKEN_PLUS, "+"},   {TOKEN_MINUS, "-"},   {TOKEN_STAR, "*"},
        {TOKEN_DIVIDE, "/"}, {TOKEN_PERCENT, "%"},
    };
    test_tokens("arithmetic operators", "+ - * / %", e, 5);
  }
  {
    Expected e[] = {
        {TOKEN_LT, "<"},     {TOKEN_LT_EQ, "<="}, {TOKEN_GT, ">"},
        {TOKEN_GT_EQ, ">="}, {TOKEN_EQ_EQ, "=="}, {TOKEN_BANG_EQ, "!="},
    };
    test_tokens("comparison operators", "< <= > >= == !=", e, 6);
  }
  {
    Expected e[] = {
        {TOKEN_EQ, "="},
        {TOKEN_BANG, "!"},
        {TOKEN_AND_AND, "&&"},
        {TOKEN_OR_OR, "||"},
    };
    test_tokens("assignment and logical", "= ! && ||", e, 4);
  }
  {
    Expected e[] = {{TOKEN_PLUS_PLUS, "++"}, {TOKEN_MINUS_MINUS, "--"}};
    test_tokens("increment and decrement", "++ --", e, 2);
  }

  printf("\n--- literals ---\n");
  {
    Expected e[] = {
        {TOKEN_INT_LITERAL, "0"},
        {TOKEN_INT_LITERAL, "42"},
        {TOKEN_INT_LITERAL, "12345"},
    };
    test_tokens("integer literals", "0 42 12345", e, 3);
  }
  {
    Expected e[] = {
        {TOKEN_FLOAT_LITERAL, "3.14"},
        {TOKEN_FLOAT_LITERAL, "0.5"},
        {TOKEN_FLOAT_LITERAL, "100.0"},
    };
    test_tokens("float literals", "3.14 0.5 100.0", e, 3);
  }
  {
    Expected e[] = {{TOKEN_STRING_LITERAL, "\"hello\""},
                    {TOKEN_STRING_LITERAL, "\"world\""}};
    test_tokens("string literal", "\"hello\" \"world\"", e, 2);
  }
  {
    Expected e[] = {{TOKEN_STRING_LITERAL, "\"he\\\"llo\""}};
    test_tokens("string with escape", "\"he\\\"llo\"", e, 1);
  }
  {
    Expected e[] = {{TOKEN_CHAR_LITERAL, "'a'"}, {TOKEN_CHAR_LITERAL, "'Z'"}};
    test_tokens("char literal", "'a' 'Z'", e, 2);
  }
  {
    Expected e[] = {{TOKEN_CHAR_LITERAL, "'\\n'"}};
    test_tokens("escaped char literal", "'\\n'", e, 1);
  }

  printf("\n--- identifiers ---\n");
  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "foo"},
        {TOKEN_IDENTIFIER, "bar"},
        {TOKEN_IDENTIFIER, "_baz"},
        {TOKEN_IDENTIFIER, "x1"},
    };
    test_tokens("simple identifiers", "foo bar _baz x1", e, 4);
  }

  printf("\n--- keywords ---\n");
  {
    Expected e[] = {
        {TOKEN_KEYWORD_INT, "int"},       {TOKEN_KEYWORD_FLOAT, "float"},
        {TOKEN_KEYWORD_CHAR, "char"},     {TOKEN_KEYWORD_VOID, "void"},
        {TOKEN_KEYWORD_RETURN, "return"}, {TOKEN_KEYWORD_IF, "if"},
        {TOKEN_KEYWORD_ELSE, "else"},     {TOKEN_KEYWORD_WHILE, "while"},
        {TOKEN_KEYWORD_FOR, "for"},
    };
    test_tokens("all keywords", "int float char void return if else while for",
                e, 9);
  }
  {
    Expected e[] = {{TOKEN_IDENTIFIER, "integer"}, {TOKEN_IDENTIFIER, "iff"}};
    test_tokens("keyword prefix is identifier", "integer iff", e, 2);
  }

  printf("\n--- full expressions ---\n");
  {
    Expected e[] = {
        {TOKEN_KEYWORD_INT, "int"}, {TOKEN_IDENTIFIER, "x"}, {TOKEN_EQ, "="},
        {TOKEN_INT_LITERAL, "42"},  {TOKEN_SEMICOLON, ";"},
    };
    test_tokens("variable declaration", "int x = 42;", e, 5);
  }
  {
    Expected e[] = {
        {TOKEN_KEYWORD_INT, "int"}, {TOKEN_IDENTIFIER, "main"},
        {TOKEN_LPAREN, "("},        {TOKEN_KEYWORD_VOID, "void"},
        {TOKEN_RPAREN, ")"},
    };
    test_tokens("function signature", "int main(void)", e, 5);
  }
  {
    Expected e[] = {
        {TOKEN_KEYWORD_IF, "if"},  {TOKEN_LPAREN, "("},
        {TOKEN_IDENTIFIER, "x"},   {TOKEN_GT_EQ, ">="},
        {TOKEN_INT_LITERAL, "10"}, {TOKEN_AND_AND, "&&"},
        {TOKEN_IDENTIFIER, "y"},   {TOKEN_BANG_EQ, "!="},
        {TOKEN_INT_LITERAL, "0"},  {TOKEN_RPAREN, ")"},
    };
    test_tokens("if statement", "if (x >= 10 && y != 0)", e, 10);
  }
  {
    Expected e[] = {
        {TOKEN_KEYWORD_FOR, "for"},
        {TOKEN_LPAREN, "("},
        {TOKEN_KEYWORD_INT, "int"},
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_EQ, "="},
        {TOKEN_INT_LITERAL, "0"},
        {TOKEN_SEMICOLON, ";"},
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_LT, "<"},
        {TOKEN_INT_LITERAL, "10"},
        {TOKEN_SEMICOLON, ";"},
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_EQ, "="},
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_PLUS, "+"},
        {TOKEN_INT_LITERAL, "1"},
        {TOKEN_RPAREN, ")"},
    };
    test_tokens("for loop", "for (int i = 0; i < 10; i = i + 1)", e, 17);
  }

  printf("\n--- whitespace ---\n");
  test_tokens("empty input", "", NULL, 0);
  test_tokens("only whitespace", "   \t\n  ", NULL, 0);

  printf("\n--- no spaces ---\n");
  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "x"}, {TOKEN_PLUS, "+"}, {TOKEN_IDENTIFIER, "y"}};
    test_tokens("x+y", "x+y", e, 3);
  }
  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "a"}, {TOKEN_GT_EQ, ">="}, {TOKEN_IDENTIFIER, "b"}};
    test_tokens("a>=b", "a>=b", e, 3);
  }
  {
    Expected e[] = {{TOKEN_IDENTIFIER, "i"}, {TOKEN_PLUS_PLUS, "++"}};
    test_tokens("i++", "i++", e, 2);
  }
  {
    Expected e[] = {{TOKEN_IDENTIFIER, "i"},
                    {TOKEN_MINUS_MINUS, "--"},
                    {TOKEN_SEMICOLON, ";"}};
    test_tokens("i--;", "i--;", e, 3);
  }
  {
    Expected e[] = {{TOKEN_BANG, "!"}, {TOKEN_IDENTIFIER, "x"}};
    test_tokens("!x", "!x", e, 2);
  }
  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "a"},  {TOKEN_EQ_EQ, "=="},
        {TOKEN_INT_LITERAL, "0"}, {TOKEN_AND_AND, "&&"},
        {TOKEN_IDENTIFIER, "b"},  {TOKEN_BANG_EQ, "!="},
        {TOKEN_INT_LITERAL, "1"},
    };
    test_tokens("a==0&&b!=1", "a==0&&b!=1", e, 7);
  }
  {
    Expected e[] = {
        {TOKEN_INT_LITERAL, "3"}, {TOKEN_STAR, "*"}, {TOKEN_LPAREN, "("},
        {TOKEN_INT_LITERAL, "4"}, {TOKEN_PLUS, "+"}, {TOKEN_INT_LITERAL, "5"},
        {TOKEN_RPAREN, ")"},
    };
    test_tokens("3*(4+5)", "3*(4+5)", e, 7);
  }

  printf("\n--- errors ---\n");
  test_error("unexpected character", "@", "Unexpected character");
  test_error("bare ampersand", "&x", "Expected '&&', got single '&'");
  test_error("bare pipe", "|x", "Expected '||', got single '|'");
  test_error("unterminated string", "\"hello",
             "Unterminated string literal, missing closing '\"'");
  test_error("unterminated char", "'a",
             "Unterminated char literal, missing closing '\\''");

  // ===== PARSER TESTS (fixture-based) =====
  printf("\n===== PARSER TESTS =====\n");

  printf("\n--- good fixtures ---\n");

  test_parse_file("empty function", "test/fixtures/good/empty_function.c",
                  "Program\n"
                  "  FuncDef 'void'\n"
                  "    Identifier 'main'\n");

  test_parse_file("return literal", "test/fixtures/good/return_literal.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'main'\n"
                  "    ReturnStmt\n"
                  "      IntLiteral '0'\n");

  test_parse_file("function with params",
                  "test/fixtures/good/function_with_params.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'add'\n"
                  "    Param 'int'\n"
                  "      Identifier 'a'\n"
                  "    Param 'int'\n"
                  "      Identifier 'b'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '+'\n"
                  "        Identifier 'a'\n"
                  "        Identifier 'b'\n");

  test_parse_file("operator precedence",
                  "test/fixtures/good/operator_precedence.c",
                  // 1 + 2 * 3  =>  1 + (2 * 3)
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '+'\n"
                  "        IntLiteral '1'\n"
                  "        BinaryExpr '*'\n"
                  "          IntLiteral '2'\n"
                  "          IntLiteral '3'\n");

  test_parse_file("left associativity",
                  "test/fixtures/good/left_associativity.c",
                  // 1 - 2 - 3  =>  (1 - 2) - 3
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '-'\n"
                  "        BinaryExpr '-'\n"
                  "          IntLiteral '1'\n"
                  "          IntLiteral '2'\n"
                  "        IntLiteral '3'\n");

  test_parse_file("unary operators", "test/fixtures/good/unary_operators.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      UnaryExpr '-'\n"
                  "        IntLiteral '1'\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'g'\n"
                  "    Param 'int'\n"
                  "      Identifier 'x'\n"
                  "    ReturnStmt\n"
                  "      UnaryExpr '!'\n"
                  "        Identifier 'x'\n");

  test_parse_file("parenthesized", "test/fixtures/good/parenthesized.c",
                  // (1 + 2) * 3 => parens override precedence
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '*'\n"
                  "        BinaryExpr '+'\n"
                  "          IntLiteral '1'\n"
                  "          IntLiteral '2'\n"
                  "        IntLiteral '3'\n");

  test_parse_file("variable declaration",
                  "test/fixtures/good/variable_declaration.c",
                  "Program\n"
                  "  FuncDef 'void'\n"
                  "    Identifier 'f'\n"
                  "    VarDecl 'int'\n"
                  "      Identifier 'x'\n"
                  "      IntLiteral '42'\n"
                  "    VarDecl 'float'\n"
                  "      Identifier 'y'\n"
                  "      FloatLiteral '3.14'\n"
                  "    VarDecl 'int'\n"
                  "      Identifier 'z'\n");

  test_parse_file("function calls", "test/fixtures/good/function_calls.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      CallExpr 'add'\n"
                  "        IntLiteral '1'\n"
                  "        IntLiteral '2'\n");

  test_parse_file("multiple functions",
                  "test/fixtures/good/multiple_functions.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'add'\n"
                  "    Param 'int'\n"
                  "      Identifier 'a'\n"
                  "    Param 'int'\n"
                  "      Identifier 'b'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '+'\n"
                  "        Identifier 'a'\n"
                  "        Identifier 'b'\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'main'\n"
                  "    ReturnStmt\n"
                  "      CallExpr 'add'\n"
                  "        IntLiteral '1'\n"
                  "        IntLiteral '2'\n");

  test_parse_file("complex expression",
                  "test/fixtures/good/complex_expression.c",
                  // a + b * c - d / e  =>  (a + (b*c)) - (d/e)
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    Param 'int'\n"
                  "      Identifier 'a'\n"
                  "    Param 'int'\n"
                  "      Identifier 'b'\n"
                  "    Param 'int'\n"
                  "      Identifier 'c'\n"
                  "    Param 'int'\n"
                  "      Identifier 'd'\n"
                  "    Param 'int'\n"
                  "      Identifier 'e'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '-'\n"
                  "        BinaryExpr '+'\n"
                  "          Identifier 'a'\n"
                  "          BinaryExpr '*'\n"
                  "            Identifier 'b'\n"
                  "            Identifier 'c'\n"
                  "        BinaryExpr '/'\n"
                  "          Identifier 'd'\n"
                  "          Identifier 'e'\n");

  test_parse_file("logical operators", "test/fixtures/good/logical_operators.c",
                  // a && b || c  =>  (a && b) || c
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    Param 'int'\n"
                  "      Identifier 'a'\n"
                  "    Param 'int'\n"
                  "      Identifier 'b'\n"
                  "    Param 'int'\n"
                  "      Identifier 'c'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '||'\n"
                  "        BinaryExpr '&&'\n"
                  "          Identifier 'a'\n"
                  "          Identifier 'b'\n"
                  "        Identifier 'c'\n");

  test_parse_file("comparison operators",
                  "test/fixtures/good/comparison_operators.c",
                  // a >= b == 0  =>  (a >= b) == 0
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    Param 'int'\n"
                  "      Identifier 'a'\n"
                  "    Param 'int'\n"
                  "      Identifier 'b'\n"
                  "    ReturnStmt\n"
                  "      BinaryExpr '=='\n"
                  "        BinaryExpr '>='\n"
                  "          Identifier 'a'\n"
                  "          Identifier 'b'\n"
                  "        IntLiteral '0'\n");

  test_parse_file("expression statements",
                  "test/fixtures/good/expression_statements.c",
                  "Program\n"
                  "  FuncDef 'void'\n"
                  "    Identifier 'f'\n"
                  "    ExprStmt\n"
                  "      CallExpr 'foo'\n"
                  "        IntLiteral '1'\n"
                  "    ExprStmt\n"
                  "      CallExpr 'bar'\n"
                  "        IntLiteral '2'\n");

  test_parse_file("nested calls", "test/fixtures/good/nested_calls.c",
                  // add(mul(2, 3), 4)
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    ReturnStmt\n"
                  "      CallExpr 'add'\n"
                  "        CallExpr 'mul'\n"
                  "          IntLiteral '2'\n"
                  "          IntLiteral '3'\n"
                  "        IntLiteral '4'\n");

  test_parse_file("mixed statements", "test/fixtures/good/mixed_statements.c",
                  "Program\n"
                  "  FuncDef 'int'\n"
                  "    Identifier 'f'\n"
                  "    Param 'int'\n"
                  "      Identifier 'n'\n"
                  "    VarDecl 'int'\n"
                  "      Identifier 'result'\n"
                  "      BinaryExpr '+'\n"
                  "        BinaryExpr '*'\n"
                  "          Identifier 'n'\n"
                  "          IntLiteral '2'\n"
                  "        IntLiteral '1'\n"
                  "    ExprStmt\n"
                  "      CallExpr 'print'\n"
                  "        Identifier 'result'\n"
                  "    ReturnStmt\n"
                  "      Identifier 'result'\n");

  test_parse_file("string and char literals",
                  "test/fixtures/good/string_and_char.c",
                  "Program\n"
                  "  FuncDef 'void'\n"
                  "    Identifier 'f'\n"
                  "    VarDecl 'char'\n"
                  "      Identifier 'c'\n"
                  "      CharLiteral ''a''\n"
                  "    ExprStmt\n"
                  "      CallExpr 'print'\n"
                  "        StringLiteral '\"hello\"'\n");

  printf("\n--- bad fixtures (expect errors) ---\n");

  test_parse_error("missing semicolon",
                   "test/fixtures/bad/missing_semicolon.c");
  test_parse_error("missing brace", "test/fixtures/bad/missing_brace.c");
  test_parse_error("missing function name",
                   "test/fixtures/bad/missing_func_name.c");
  test_parse_error("missing open paren", "test/fixtures/bad/missing_paren.c");
  test_parse_error("missing close paren", "test/fixtures/bad/missing_rparen.c");
  test_parse_error("bad param type", "test/fixtures/bad/bad_param_type.c");

  // ===== summary =====
  printf("\n========================================\n");
  if (tests_failed == 0) {
    printf("  ALL PASSED: %d/%d tests\n", tests_passed, tests_run);
  } else {
    printf("  %d PASSED, %d FAILED out of %d tests\n", tests_passed,
           tests_failed, tests_run);
  }
  printf("========================================\n");

  return tests_failed > 0 ? 1 : 0;
}
