#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

void *memset(void *s, int c, size_t n) {
  u8 *p = s;
  for (size_t i = 0; i < n; i++) {
    p[i] = (u8)c;
  }
  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  u8 *d = dest;
  const u8 *s = src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const u8 *a = s1;
  const u8 *b = s2;
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i])
      return a[i] - b[i];
  }
  return 0;
}

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }
