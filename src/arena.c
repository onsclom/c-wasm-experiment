#include "arena.h"

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

static void arena_reset(Arena *a) { a->len = 0; }
