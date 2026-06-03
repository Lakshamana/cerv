#include "cerv/arena.h"
#include "test.h"
#include <stddef.h>
#include <string.h>

static int passed = 0, failed = 0;

/* ── arena_new ───────────────────────────────────────────────────────────── */

static void test_arena_new() {
  TEST(arena_new);

  Arena *a = arena_new(1024);

  ASSERT_NOT_NULL(a, "arena should not be NULL");
  ASSERT_NOT_NULL(a->buf, "arena buf should not be NULL");
  ASSERT_INT_EQ((int)a->size, 1024, "arena size should match requested size");
  ASSERT_INT_EQ((int)a->offset, 0, "offset should start at 0");

  arena_destroy(a);
}

/* ── arena_alloc ─────────────────────────────────────────────────────────── */

static void test_arena_alloc_basic() {
  TEST(arena_alloc_basic);

  Arena *a = arena_new(1024);
  Allocator alloc = arena_allocator(a);

  int *p = cerv_make(int, alloc);

  ASSERT_NOT_NULL(p, "allocation should succeed");
  ASSERT((char *)p >= a->buf && (char *)p < a->buf + a->size,
         "pointer should lie within arena bounds");

  arena_destroy(a);
}

static void test_arena_alloc_alignment() {
  TEST(arena_alloc_alignment);

  Arena *a = arena_new(1024);
  Allocator alloc = arena_allocator(a);

  // allocate 1 byte, then a larger type — second must be aligned
  cerv_alloc(char, 1, alloc);
  int *p = cerv_make(int, alloc);

  ASSERT_NOT_NULL(p, "second alloc should succeed");
  ASSERT(((size_t)p % CERV_DEFAULT_ALIGNMENT) == 0,
         "allocation should be aligned to CERV_DEFAULT_ALIGNMENT");

  arena_destroy(a);
}

static void test_arena_alloc_zero() {
  TEST(arena_alloc_zero);

  Arena *a = arena_new(1024);
  Allocator alloc = arena_allocator(a);

  void *p = cerv_alloc(char, 0, alloc);
  ASSERT_NULL(p, "allocating 0 bytes should return NULL");

  arena_destroy(a);
}

static void test_arena_alloc_overflow() {
  TEST(arena_alloc_overflow);

  Arena *a = arena_new(64);
  Allocator alloc = arena_allocator(a);

  void *p = cerv_alloc(char, 128, alloc);
  ASSERT_NULL(p, "allocation exceeding arena capacity should return NULL");

  arena_destroy(a);
}

static void test_arena_alloc_exact_fit() {
  TEST(arena_alloc_exact_fit);

  Arena *a = arena_new(CERV_DEFAULT_ALIGNMENT);
  Allocator alloc = arena_allocator(a);

  // exactly fills the arena (offset starts at 0, already aligned)
  void *p = cerv_alloc(char, CERV_DEFAULT_ALIGNMENT, alloc);
  ASSERT_NOT_NULL(p, "exact-fit allocation should succeed");

  // one more byte must overflow
  void *q = cerv_alloc(char, 1, alloc);
  ASSERT_NULL(q, "allocation past exact fill should return NULL");

  arena_destroy(a);
}

/* ── arena_reset ─────────────────────────────────────────────────────────── */

static void test_arena_reset_clears_offset() {
  TEST(arena_reset_clears_offset);

  Arena *a = arena_new(1024);
  Allocator alloc = arena_allocator(a);

  cerv_make(int, alloc);
  ASSERT((int)a->offset > 0, "offset should advance after allocation");

  arena_reset(a);
  ASSERT_INT_EQ((int)a->offset, 0, "offset should be 0 after reset");

  arena_destroy(a);
}

static void test_arena_reset_reuse() {
  TEST(arena_reset_reuse);

  Arena *a = arena_new(1024);
  Allocator alloc = arena_allocator(a);

  int *p1 = cerv_make(int, alloc);
  arena_reset(a);
  int *p2 = cerv_make(int, alloc);

  // both allocations start from offset 0 — must land at the same address
  ASSERT(p1 == p2, "post-reset allocation should return same address as first");

  arena_destroy(a);
}

/* ── arena_strndup ────────────────────────────────────────────────────────── */

static void test_arena_strndup_basic() {
  TEST(arena_strndup_basic);

  Arena *a = arena_new(1024);
  const char *src = "hello, cerv";
  char *copy = arena_strndup(a, src, strlen(src));

  ASSERT_NOT_NULL(copy, "arena_strndup should not return NULL");
  ASSERT_STR_EQ(copy, src, "copy should match the original string");
  ASSERT(copy != src, "copy should be a distinct pointer");

  arena_destroy(a);
}

static void test_arena_strndup_empty_string() {
  TEST(arena_strndup_empty_string);

  Arena *a = arena_new(1024);
  char *copy = arena_strndup(a, "", 0);

  ASSERT_NOT_NULL(copy, "arena_strndup of empty string should not return NULL");
  ASSERT_STR_EQ(copy, "", "copy should be an empty string");

  arena_destroy(a);
}

static void test_arena_strndup_overflow() {
  TEST(arena_strndup_overflow);

  Arena *a = arena_new(8);
  char *copy = arena_strndup(a, "this string is too long for the arena", 37);

  ASSERT_NULL(copy, "arena_strndup should return NULL when arena is full");

  arena_destroy(a);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
  typedef void (*TestFn)(void);
  struct { const char *name; TestFn fn; } tests[] = {
    {"test_arena_new",                test_arena_new},
    {"test_arena_alloc_basic",        test_arena_alloc_basic},
    {"test_arena_alloc_alignment",    test_arena_alloc_alignment},
    {"test_arena_alloc_zero",         test_arena_alloc_zero},
    {"test_arena_alloc_overflow",     test_arena_alloc_overflow},
    {"test_arena_alloc_exact_fit",    test_arena_alloc_exact_fit},
    {"test_arena_reset_clears_offset", test_arena_reset_clears_offset},
    {"test_arena_reset_reuse",        test_arena_reset_reuse},
    {"test_arena_strndup_basic",      test_arena_strndup_basic},
    {"test_arena_strndup_empty_string", test_arena_strndup_empty_string},
    {"test_arena_strndup_overflow",   test_arena_strndup_overflow},
  };
  size_t ntests = sizeof(tests) / sizeof(tests[0]);

  if (argc == 2) {
    for (size_t i = 0; i < ntests; i++) {
      if (strcmp(argv[1], tests[i].name) == 0) {
        tests[i].fn();
        printf("%s\n%d assertions passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR, passed, failed);
        return failed > 0 ? 1 : 0;
      }
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
  }

  for (size_t i = 0; i < ntests; i++)
    tests[i].fn();

  printf("%s\n%d assertions passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR,
         passed, failed);

  return failed > 0 ? 1 : 0;
}
