#include "string.h"

static bool s8_eq(s8 a, s8 b) {
  if (a.length != b.length)
    return false;
  return memcmp(a.data, b.data, a.length) == 0;
}
