#pragma once

#include "base.h"

typedef struct {
  char *data;
  size_t length;
} s8;

#define s8_lit(str) (s8){.data = (char *)(str), .length = sizeof(str) - 1}

static bool s8_eq(s8 a, s8 b);
