#pragma once

#include "base.h"

typedef struct {
  u8 *base;
  size_t cap;
  size_t len;
} Arena;

static size_t align_forward(size_t x, size_t align);
static void *arena_alloc(Arena *a, size_t size, size_t align);
static void arena_reset(Arena *a);
