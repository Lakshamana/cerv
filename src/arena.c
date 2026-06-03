#include "cerv/arena.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Arena *arena_new(size_t size) {
  Arena *a = calloc(1, sizeof(Arena));
  a->size = size;
  a->buf = calloc(1, size);

  return a;
}

void arena_reset(Arena *a) {
  a->offset = 0;
}

void arena_destroy(Arena *a) {
  free(a->buf);
  free(a);
  a = NULL;
}

static void *arena_alloc(void *ctx, size_t n) {
  if (!n) {
    return NULL;
  }

  Arena *a = ctx;
  size_t aligned = (a->offset + (CERV_DEFAULT_ALIGNMENT - 1)) &
                   ~(CERV_DEFAULT_ALIGNMENT - 1);
  if (aligned + n > a->size)
    return NULL;

  void *ptr = a->buf + aligned;
  a->offset = aligned + n;
  return ptr;
}

char *arena_strndup(Arena *a, const char *s, size_t len) {
  char *ptr = (char *)arena_alloc(a, len + 1);
  if (ptr == NULL) {
    return NULL;
  }

  memcpy(ptr, s, len);
  ptr[len] = '\0';
  return ptr;
}

Allocator arena_allocator(Arena *a) { return (Allocator){arena_alloc, a}; }
