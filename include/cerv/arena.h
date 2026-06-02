#ifndef CERV_ARENA_H
#define CERV_ARENA_H

#define CERV_DEFAULT_ALIGNMENT (_Alignof(max_align_t))

#define cerv_alloc(T, n, a) ((T *)((a).alloc((a).ctx, sizeof(T) * n)))
#define cerv_make(T, a) cerv_alloc(T, 1, a)

#include <stddef.h>

typedef struct {
  char  *buf;
  size_t size;
  size_t offset;
} Arena;

typedef struct {
  void *(*alloc)(void* ctx, size_t size);
  void *ctx;
} Allocator;

Arena *arena_new(size_t size);
void  arena_reset(Arena *a);
void  arena_destroy(Arena *a);

char  *arena_strndup(Arena *a, const char *s, size_t len);

Allocator arena_allocator(Arena *a);

#endif // !CERV_ARENA_H
