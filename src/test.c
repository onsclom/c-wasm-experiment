#include "base/base_inc.c"
#include "tokenize.c"
#include <stdio.h>

static u8 arena_mem[64 * 1024];
static Arena arena;

static i32 tests_run = 0;
static i32 tests_passed = 0;
static i32 tests_failed = 0;

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

  TokenizeResult result = tokenize(source, &arena);

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
      // check text match
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
        if (tok_text[j] != exp_text[j]) {
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
        s8 name = token_type_name(tok->type);
        snprintf(got_buf, sizeof(got_buf), "%.*s", (i32)name.length,
                 (char *)name.data);
        size_t tok_len = tok->end - tok->start;
        if (tok_len >= sizeof(got_text))
          tok_len = sizeof(got_text) - 1;
        for (size_t j = 0; j < tok_len; j++)
          got_text[j] = source.data[tok->start + j];
        got_text[tok_len] = '\0';
      }
      if (i < expected_count) {
        s8 name = token_type_name(expected[i].type);
        snprintf(exp_buf, sizeof(exp_buf), "%.*s", (i32)name.length,
                 (char *)name.data);
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

  TokenizeResult result = tokenize(source, &arena);

  if (result.ok) {
    tests_failed++;
    printf("  FAIL  %s\n", name);
    printf("        expected error but tokenize succeeded with %zu tokens\n",
           result.count);
    return;
  }

  // check error message if provided
  if (expected_msg) {
    s8 msg = result.error_message;
    size_t exp_len = 0;
    while (expected_msg[exp_len])
      exp_len++;
    bool match = msg.length == exp_len;
    if (match) {
      for (size_t i = 0; i < exp_len; i++) {
        if (msg.data[i] != expected_msg[i]) {
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

int main(void) {
  arena = (Arena){
      .base = arena_mem,
      .cap = sizeof(arena_mem),
      .len = 0,
  };

  printf("--- punctuation ---\n");

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
    Expected e[] = {
        {TOKEN_PLUS_PLUS, "++"},
        {TOKEN_MINUS_MINUS, "--"},
    };
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
    Expected e[] = {
        {TOKEN_STRING_LITERAL, "\"hello\""},
        {TOKEN_STRING_LITERAL, "\"world\""},
    };
    test_tokens("string literal", "\"hello\" \"world\"", e, 2);
  }

  {
    Expected e[] = {{TOKEN_STRING_LITERAL, "\"he\\\"llo\""}};
    test_tokens("string with escape", "\"he\\\"llo\"", e, 1);
  }

  {
    Expected e[] = {
        {TOKEN_CHAR_LITERAL, "'a'"},
        {TOKEN_CHAR_LITERAL, "'Z'"},
    };
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
    Expected e[] = {
        {TOKEN_IDENTIFIER, "integer"},
        {TOKEN_IDENTIFIER, "iff"},
    };
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
        {TOKEN_IDENTIFIER, "x"},
        {TOKEN_PLUS, "+"},
        {TOKEN_IDENTIFIER, "y"},
    };
    test_tokens("x+y", "x+y", e, 3);
  }

  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "a"},
        {TOKEN_GT_EQ, ">="},
        {TOKEN_IDENTIFIER, "b"},
    };
    test_tokens("a>=b", "a>=b", e, 3);
  }

  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_PLUS_PLUS, "++"},
    };
    test_tokens("i++", "i++", e, 2);
  }

  {
    Expected e[] = {
        {TOKEN_IDENTIFIER, "i"},
        {TOKEN_MINUS_MINUS, "--"},
        {TOKEN_SEMICOLON, ";"},
    };
    test_tokens("i--;", "i--;", e, 3);
  }

  {
    Expected e[] = {
        {TOKEN_BANG, "!"},
        {TOKEN_IDENTIFIER, "x"},
    };
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

  // summary
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
