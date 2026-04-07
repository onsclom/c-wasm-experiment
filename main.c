#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  for (size_t i = 0; i < n; i++) {
    p[i] = (unsigned char)c;
  }
  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

// ARENA

typedef struct {
  unsigned char *base;
  size_t cap;
  size_t len;
} Arena;

static size_t align_forward(size_t x, size_t align) {
  size_t mask = align - 1;
  return (x + mask) & ~mask;
}

static void *arena_alloc(Arena *a, size_t size, size_t align) {
  size_t start = align_forward(a->len, align);
  size_t end = start + size;

  if (end > a->cap) {
    return NULL;
  }

  void *ptr = a->base + start;
  a->len = end;
  return ptr;
}

static void *arena_alloc_zero(Arena *a, size_t size, size_t align) {
  void *ptr = arena_alloc(a, size, align);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static void arena_reset(Arena *a) { a->len = 0; }

static void arena_append(Arena *a, const void *data, size_t size) {
  void *ptr = arena_alloc(a, size, 1);
  if (ptr) {
    memcpy(ptr, data, size);
  }
}

// STRINGS

typedef struct {
  char *data;
  size_t length;
} s8;

#define s8_lit(str) (s8){.data = (char *)(str), .length = sizeof(str) - 1}

// LEXER

typedef enum {
  TOKEN_IDENTIFIER,
  TOKEN_INT_LITERAL,
  TOKEN_FLOAT_LITERAL,
  TOKEN_STRING_LITERAL,
  TOKEN_CHAR_LITERAL,

  // operators
  TOKEN_PLUS,    // +
  TOKEN_MINUS,   // -
  TOKEN_STAR,    // mul or value dereference
  TOKEN_DIVIDE,  // /
  TOKEN_PERCENT, // %

  TOKEN_EQ,      // =
  TOKEN_EQ_EQ,   // ==
  TOKEN_BANG,    // !
  TOKEN_BANG_EQ, // !=

  TOKEN_LT,    // <
  TOKEN_LT_EQ, // <=
  TOKEN_GT,    // >
  TOKEN_GT_EQ, // >=

  // TOKEN_AND,     // &
  TOKEN_AND_AND, // &&
  // TOKEN_OR,      // |
  TOKEN_OR_OR, // ||

  // punctuation
  TOKEN_LPAREN,   // (
  TOKEN_RPAREN,   // )
  TOKEN_LBRACE,   // {
  TOKEN_RBRACE,   // }
  TOKEN_LBRACKET, // [
  TOKEN_RBRACKET, // ]

  TOKEN_COMMA,     // ,
  TOKEN_SEMICOLON, // ;
  TOKEN_COLON,     // :

  TOKEN_KEYWORD_INT,
  TOKEN_KEYWORD_FLOAT,
  TOKEN_KEYWORD_CHAR,
  TOKEN_KEYWORD_VOID,
  TOKEN_KEYWORD_RETURN,
  TOKEN_KEYWORD_IF,
  TOKEN_KEYWORD_ELSE,
  TOKEN_KEYWORD_WHILE,
  TOKEN_KEYWORD_FOR,

  // TOKEN_KEYWORD_STRUCT,
  // TOKEN_KEYWORD_TYPEDEF,

  // TOKEN_DOT,         // .
  // TOKEN_ARROW,       // ->

  // TOKEN_QUESTION,    // ?

  // TOKEN_CARET,       // ^
  // TOKEN_TILDE,       // ~

} TokenType;

static const char *token_type_name(TokenType type) {
  switch (type) {
  case TOKEN_IDENTIFIER:
    return "IDENTIFIER";
  case TOKEN_INT_LITERAL:
    return "INT_LITERAL";
  case TOKEN_FLOAT_LITERAL:
    return "FLOAT_LITERAL";
  case TOKEN_STRING_LITERAL:
    return "STRING_LITERAL";
  case TOKEN_CHAR_LITERAL:
    return "CHAR_LITERAL";
  case TOKEN_PLUS:
    return "PLUS";
  case TOKEN_MINUS:
    return "MINUS";
  case TOKEN_STAR:
    return "STAR";
  case TOKEN_DIVIDE:
    return "DIVIDE";
  case TOKEN_PERCENT:
    return "PERCENT";
  case TOKEN_EQ:
    return "EQ";
  case TOKEN_EQ_EQ:
    return "EQ_EQ";
  case TOKEN_BANG:
    return "BANG";
  case TOKEN_BANG_EQ:
    return "BANG_EQ";
  case TOKEN_LT:
    return "LT";
  case TOKEN_LT_EQ:
    return "LT_EQ";
  case TOKEN_GT:
    return "GT";
  case TOKEN_GT_EQ:
    return "GT_EQ";
  case TOKEN_AND_AND:
    return "AND_AND";
  case TOKEN_OR_OR:
    return "OR_OR";
  case TOKEN_LPAREN:
    return "LPAREN";
  case TOKEN_RPAREN:
    return "RPAREN";
  case TOKEN_LBRACE:
    return "LBRACE";
  case TOKEN_RBRACE:
    return "RBRACE";
  case TOKEN_LBRACKET:
    return "LBRACKET";
  case TOKEN_RBRACKET:
    return "RBRACKET";
  case TOKEN_COMMA:
    return "COMMA";
  case TOKEN_SEMICOLON:
    return "SEMICOLON";
  case TOKEN_COLON:
    return "COLON";
  case TOKEN_KEYWORD_INT:
    return "KW_INT";
  case TOKEN_KEYWORD_FLOAT:
    return "KW_FLOAT";
  case TOKEN_KEYWORD_CHAR:
    return "KW_CHAR";
  case TOKEN_KEYWORD_VOID:
    return "KW_VOID";
  case TOKEN_KEYWORD_RETURN:
    return "KW_RETURN";
  case TOKEN_KEYWORD_IF:
    return "KW_IF";
  case TOKEN_KEYWORD_ELSE:
    return "KW_ELSE";
  case TOKEN_KEYWORD_WHILE:
    return "KW_WHILE";
  case TOKEN_KEYWORD_FOR:
    return "KW_FOR";
  }
  return "UNKNOWN";
}

typedef struct {
  TokenType type;
  // slice into the source string
  size_t start;
  size_t end;
} Token;

typedef struct {
  Token *tokens;
  size_t count;

  bool ok;
  s8 error_message;
} TokenizeResult;

TokenizeResult tokenize(s8 source, Arena *arena) {
  size_t pos = 0;

  // tokens will be appended contiguously starting here
  size_t tokens_start = align_forward(arena->len, _Alignof(Token));
  TokenizeResult result = {
      .ok = true,
      .tokens = (Token *)(arena->base + tokens_start),
      .count = 0,
  };

  while (pos < source.length) {
    const char cur = source.data[pos];

    switch (cur) {
    case ';': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_SEMICOLON, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case '(': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_LPAREN, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case ')': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_RPAREN, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case '{': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_LBRACE, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case '}': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_RBRACE, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case '[': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_LBRACKET, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case ']': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_RBRACKET, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }
    case ',': {
      Token *t = arena_alloc(arena, sizeof(Token), _Alignof(Token));
      *t = (Token){TOKEN_COMMA, pos, pos + 1};
      result.count++;
      pos++;
      break;
    }

    case ' ':
    case '\t':
    case '\n': {
      pos++;
      break;
    }

    default: {
      result.ok = false;
      result.error_message = s8_lit("Unexpected character");
      return result;
    }
    }
  }
  return result;
}

static unsigned char arena_mem[64 * 1024];

int main() {
  Arena arena = {
      .base = arena_mem,
      .cap = sizeof(arena_mem),
      .len = 0,
  };

  {
    s8 source = s8_lit("{;([]},");

    TokenizeResult result = tokenize(source, &arena);

    if (!result.ok) {
      printf("Error: %.*s\n", (int)result.error_message.length,
             result.error_message.data);
      return 1;
    }

    printf("%zu tokens:\n", result.count);

    for (size_t i = 0; i < result.count; i++) {
      Token *tok = &result.tokens[i];
      printf("  %-12s \"%.*s\"\n", token_type_name(tok->type),
             (int)(tok->end - tok->start), source.data + tok->start);
    }
  }

  return 0;
}
