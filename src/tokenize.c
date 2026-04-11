#include "tokenize.h"

static const s8 token_type_names[TOKEN_COUNT] = {
    [TOKEN_IDENTIFIER] = s8_lit("IDENTIFIER"),
    [TOKEN_INT_LITERAL] = s8_lit("INT_LITERAL"),
    [TOKEN_FLOAT_LITERAL] = s8_lit("FLOAT_LITERAL"),
    [TOKEN_STRING_LITERAL] = s8_lit("STRING_LITERAL"),
    [TOKEN_CHAR_LITERAL] = s8_lit("CHAR_LITERAL"),
    [TOKEN_PLUS] = s8_lit("PLUS"),
    [TOKEN_PLUS_PLUS] = s8_lit("PLUS_PLUS"),
    [TOKEN_MINUS] = s8_lit("MINUS"),
    [TOKEN_MINUS_MINUS] = s8_lit("MINUS_MINUS"),
    [TOKEN_STAR] = s8_lit("STAR"),
    [TOKEN_DIVIDE] = s8_lit("DIVIDE"),
    [TOKEN_PERCENT] = s8_lit("PERCENT"),
    [TOKEN_EQ] = s8_lit("EQ"),
    [TOKEN_EQ_EQ] = s8_lit("EQ_EQ"),
    [TOKEN_BANG] = s8_lit("BANG"),
    [TOKEN_BANG_EQ] = s8_lit("BANG_EQ"),
    [TOKEN_LT] = s8_lit("LT"),
    [TOKEN_LT_EQ] = s8_lit("LT_EQ"),
    [TOKEN_GT] = s8_lit("GT"),
    [TOKEN_GT_EQ] = s8_lit("GT_EQ"),
    [TOKEN_AND_AND] = s8_lit("AND_AND"),
    [TOKEN_OR_OR] = s8_lit("OR_OR"),
    [TOKEN_LPAREN] = s8_lit("LPAREN"),
    [TOKEN_RPAREN] = s8_lit("RPAREN"),
    [TOKEN_LBRACE] = s8_lit("LBRACE"),
    [TOKEN_RBRACE] = s8_lit("RBRACE"),
    [TOKEN_LBRACKET] = s8_lit("LBRACKET"),
    [TOKEN_RBRACKET] = s8_lit("RBRACKET"),
    [TOKEN_COMMA] = s8_lit("COMMA"),
    [TOKEN_SEMICOLON] = s8_lit("SEMICOLON"),
    [TOKEN_COLON] = s8_lit("COLON"),
    [TOKEN_KEYWORD_INT] = s8_lit("KW_INT"),
    [TOKEN_KEYWORD_FLOAT] = s8_lit("KW_FLOAT"),
    [TOKEN_KEYWORD_CHAR] = s8_lit("KW_CHAR"),
    [TOKEN_KEYWORD_VOID] = s8_lit("KW_VOID"),
    [TOKEN_KEYWORD_RETURN] = s8_lit("KW_RETURN"),
    [TOKEN_KEYWORD_IF] = s8_lit("KW_IF"),
    [TOKEN_KEYWORD_ELSE] = s8_lit("KW_ELSE"),
    [TOKEN_KEYWORD_WHILE] = s8_lit("KW_WHILE"),
    [TOKEN_KEYWORD_FOR] = s8_lit("KW_FOR"),
};

s8 token_type_name(TokenType type) {
  if (type >= 0 && type < TOKEN_COUNT && token_type_names[type].length > 0) {
    return token_type_names[type];
  }
  return s8_lit("UNKNOWN");
}

size_t skip_char_or_escape(const u8 *src, size_t pos, size_t len) {
  if (pos < len && src[pos] == '\\')
    pos++;
  if (pos < len)
    pos++;
  return pos;
}

void emit_token(Token *tokens, TokenizeResult *result, TokenType type,
                size_t start, size_t end) {
  tokens[result->count] = (Token){type, start, end};
  result->count++;
}

TokenizeResult tokenize_fail(s8 msg, size_t pos) {
  return (TokenizeResult){.ok = false, .error_message = msg, .error_pos = pos};
}

TokenizeResult tokenize(Arena *arena, s8 source) {
  size_t pos = 0;
  size_t len = source.length;
  const u8 *src = source.data;

  // worst case is 1 token per char
  Token *tokens = arena_alloc(arena, len * sizeof(Token));
  TokenizeResult result = {
      .ok = true,
      .tokens = tokens,
      .count = 0,
  };

  while (pos < len) {
    u8 cur = src[pos];
    u8 next = (pos + 1 < len) ? src[pos + 1] : '\0';

    if (cur == ' ' || cur == '\t' || cur == '\n' || cur == '\r') {
      pos++;
      continue;
    }

    switch (cur) {
    case ';':
      emit_token(tokens, &result, TOKEN_SEMICOLON, pos, pos + 1);
      pos++;
      continue;
    case '(':
      emit_token(tokens, &result, TOKEN_LPAREN, pos, pos + 1);
      pos++;
      continue;
    case ')':
      emit_token(tokens, &result, TOKEN_RPAREN, pos, pos + 1);
      pos++;
      continue;
    case '{':
      emit_token(tokens, &result, TOKEN_LBRACE, pos, pos + 1);
      pos++;
      continue;
    case '}':
      emit_token(tokens, &result, TOKEN_RBRACE, pos, pos + 1);
      pos++;
      continue;
    case '[':
      emit_token(tokens, &result, TOKEN_LBRACKET, pos, pos + 1);
      pos++;
      continue;
    case ']':
      emit_token(tokens, &result, TOKEN_RBRACKET, pos, pos + 1);
      pos++;
      continue;
    case ',':
      emit_token(tokens, &result, TOKEN_COMMA, pos, pos + 1);
      pos++;
      continue;
    case ':':
      emit_token(tokens, &result, TOKEN_COLON, pos, pos + 1);
      pos++;
      continue;
    case '+':
      if (next == '+') {
        emit_token(tokens, &result, TOKEN_PLUS_PLUS, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_PLUS, pos, pos + 1);
        pos++;
      }
      continue;
    case '-':
      if (next == '-') {
        emit_token(tokens, &result, TOKEN_MINUS_MINUS, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_MINUS, pos, pos + 1);
        pos++;
      }
      continue;
    case '*':
      emit_token(tokens, &result, TOKEN_STAR, pos, pos + 1);
      pos++;
      continue;
    case '/':
      emit_token(tokens, &result, TOKEN_DIVIDE, pos, pos + 1);
      pos++;
      continue;
    case '%':
      emit_token(tokens, &result, TOKEN_PERCENT, pos, pos + 1);
      pos++;
      continue;
    case '=':
      if (next == '=') {
        emit_token(tokens, &result, TOKEN_EQ_EQ, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_EQ, pos, pos + 1);
        pos++;
      }
      continue;
    case '!':
      if (next == '=') {
        emit_token(tokens, &result, TOKEN_BANG_EQ, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_BANG, pos, pos + 1);
        pos++;
      }
      continue;
    case '<':
      if (next == '=') {
        emit_token(tokens, &result, TOKEN_LT_EQ, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_LT, pos, pos + 1);
        pos++;
      }
      continue;
    case '>':
      if (next == '=') {
        emit_token(tokens, &result, TOKEN_GT_EQ, pos, pos + 2);
        pos += 2;
      } else {
        emit_token(tokens, &result, TOKEN_GT, pos, pos + 1);
        pos++;
      }
      continue;
    case '&':
      if (next == '&') {
        emit_token(tokens, &result, TOKEN_AND_AND, pos, pos + 2);
        pos += 2;
        continue;
      }
      return tokenize_fail(s8_lit("Expected '&&', got single '&'"), pos);
    case '|':
      if (next == '|') {
        emit_token(tokens, &result, TOKEN_OR_OR, pos, pos + 2);
        pos += 2;
        continue;
      }
      return tokenize_fail(s8_lit("Expected '||', got single '|'"), pos);
    default:
      break;
    }

    if (is_digit(cur)) {
      size_t start = pos;
      while (pos < len && is_digit(src[pos]))
        pos++;
      if (pos < len && src[pos] == '.') {
        pos++;
        while (pos < len && is_digit(src[pos]))
          pos++;
        emit_token(tokens, &result, TOKEN_FLOAT_LITERAL, start, pos);
      } else {
        emit_token(tokens, &result, TOKEN_INT_LITERAL, start, pos);
      }
      continue;
    }

    if (cur == '"') {
      size_t start = pos;
      pos++;
      while (pos < len && src[pos] != '"')
        pos = skip_char_or_escape(src, pos, len);
      if (pos >= len)
        return tokenize_fail(
            s8_lit("Unterminated string literal, missing closing '\"'"), start);
      pos++;
      emit_token(tokens, &result, TOKEN_STRING_LITERAL, start, pos);
      continue;
    }

    if (cur == '\'') {
      size_t start = pos;
      pos++;
      pos = skip_char_or_escape(src, pos, len);
      if (pos >= len || src[pos] != '\'')
        return tokenize_fail(
            s8_lit("Unterminated char literal, missing closing '\\''"), start);
      pos++;
      emit_token(tokens, &result, TOKEN_CHAR_LITERAL, start, pos);
      continue;
    }

    if (is_alpha(cur) || cur == '_') {
      size_t start = pos;
      while (pos < len && (is_alnum(src[pos]) || src[pos] == '_'))
        pos++;
      s8 ident = {.data = (u8 *)(src + start), .length = pos - start};
      TokenType type = TOKEN_IDENTIFIER;
      if (s8_eq(ident, s8_lit("int")))
        type = TOKEN_KEYWORD_INT;
      else if (s8_eq(ident, s8_lit("float")))
        type = TOKEN_KEYWORD_FLOAT;
      else if (s8_eq(ident, s8_lit("char")))
        type = TOKEN_KEYWORD_CHAR;
      else if (s8_eq(ident, s8_lit("void")))
        type = TOKEN_KEYWORD_VOID;
      else if (s8_eq(ident, s8_lit("return")))
        type = TOKEN_KEYWORD_RETURN;
      else if (s8_eq(ident, s8_lit("if")))
        type = TOKEN_KEYWORD_IF;
      else if (s8_eq(ident, s8_lit("else")))
        type = TOKEN_KEYWORD_ELSE;
      else if (s8_eq(ident, s8_lit("while")))
        type = TOKEN_KEYWORD_WHILE;
      else if (s8_eq(ident, s8_lit("for")))
        type = TOKEN_KEYWORD_FOR;
      emit_token(tokens, &result, type, start, pos);
      continue;
    }

    return tokenize_fail(s8_lit("Unexpected character"), pos);
  }
  return result;
}
