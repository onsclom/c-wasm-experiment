#pragma once

#include "base/base_inc.h"

typedef enum {
  TOKEN_IDENTIFIER,
  TOKEN_INT_LITERAL,
  TOKEN_FLOAT_LITERAL,
  TOKEN_STRING_LITERAL,
  TOKEN_CHAR_LITERAL,

  // operators
  TOKEN_PLUS,        // +
  TOKEN_PLUS_PLUS,   // ++
  TOKEN_MINUS,       // -
  TOKEN_MINUS_MINUS, // --
  TOKEN_STAR,        // mul or value dereference
  TOKEN_DIVIDE,      // /
  TOKEN_PERCENT,     // %

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

  TOKEN_COUNT,
} TokenType;

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
  size_t error_pos;
} TokenizeResult;

s8 token_type_name(TokenType type);
TokenizeResult tokenize(Arena *arena, s8 source);
