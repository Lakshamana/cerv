#include "cerv/arena.h"
#include "cerv/response.h"
#include "test.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

/* ── cerv_response_new ───────────────────────────────────────────────────── */

static void test_response_new() {
  TEST(cerv_response_new);

  Arena *a = arena_new(4096);
  Allocator alloc = arena_allocator(a);

  CervResponse *res = cerv_response_new(alloc);

  ASSERT_NOT_NULL(res, "response should not be NULL");
  ASSERT_INT_EQ((int)res->headers_count, 0, "headers_count should start at 0");

  arena_destroy(a);
}

/* ── cerv_response_set_status ────────────────────────────────────────────── */

static void test_response_set_status() {
  TEST(cerv_response_set_status);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));

  cerv_response_set_status(res, 201);
  ASSERT_INT_EQ(res->status, 201, "status should be 201");

  arena_destroy(a);
}

/* ── cerv_response_set_body ──────────────────────────────────────────────── */

static void test_response_set_body() {
  TEST(cerv_response_set_body);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));

  cerv_response_set_body(res, "hello");
  ASSERT_STR_EQ(res->body, "hello", "body should match");

  arena_destroy(a);
}

/* ── cerv_response_set_header ────────────────────────────────────────────── */

static void test_response_set_header_single() {
  TEST(cerv_response_set_header_single);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));

  cerv_response_set_header(res, "X-Foo", "bar");

  ASSERT_INT_EQ((int)res->headers_count, 1, "headers_count should be 1");
  ASSERT_STR_EQ(res->headers[0].key, "X-Foo", "header key should match");
  ASSERT_STR_EQ(res->headers[0].value, "bar", "header value should match on same slot");

  arena_destroy(a);
}

static void test_response_set_header_multiple() {
  TEST(cerv_response_set_header_multiple);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));

  cerv_response_set_header(res, "X-One", "1");
  cerv_response_set_header(res, "X-Two", "2");

  ASSERT_INT_EQ((int)res->headers_count, 2, "headers_count should be 2");
  ASSERT_STR_EQ(res->headers[0].key,   "X-One", "first header key");
  ASSERT_STR_EQ(res->headers[0].value, "1",     "first header value");
  ASSERT_STR_EQ(res->headers[1].key,   "X-Two", "second header key");
  ASSERT_STR_EQ(res->headers[1].value, "2",     "second header value");

  arena_destroy(a);
}

/* ── cerv_response_serialize ─────────────────────────────────────────────── */

static void test_serialize_status_line() {
  TEST(cerv_response_serialize_status_line);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 200);

  char *out = cerv_response_serialize(res);

  ASSERT_NOT_NULL(out, "serialized output should not be NULL");
  ASSERT(strstr(out, "200 OK") != NULL, "output should contain status line");

  arena_destroy(a);
}

static void test_serialize_body() {
  TEST(cerv_response_serialize_body);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 200);
  cerv_response_set_body(res, "hello");

  char *out = cerv_response_serialize(res);

  ASSERT(strstr(out, "Content-Length: 5") != NULL, "Content-Length should reflect body size");
  char *blank = strstr(out, "\r\n\r\n");
  ASSERT_NOT_NULL(blank, "blank line separator should be present");
  ASSERT_STR_EQ(blank + 4, "hello", "body should follow blank line");

  arena_destroy(a);
}

static void test_serialize_default_content_type() {
  TEST(cerv_response_serialize_default_content_type);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 200);

  char *out = cerv_response_serialize(res);

  ASSERT(strstr(out, "Content-Type: text/plain") != NULL,
         "default content-type should be text/plain");

  arena_destroy(a);
}

static void test_serialize_custom_headers_present() {
  TEST(cerv_response_serialize_custom_headers_present);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 200);
  cerv_response_set_body(res, "hello");
  cerv_response_set_header(res, "X-Request-Id", "abc123");
  cerv_response_set_header(res, "X-Custom", "value");

  char *out = cerv_response_serialize(res);

  ASSERT(strstr(out, "X-Request-Id: abc123\r\n") != NULL, "first custom header should be present");
  ASSERT(strstr(out, "X-Custom: value\r\n") != NULL, "second custom header should be present");

  arena_destroy(a);
}

static void test_serialize_custom_headers_before_body() {
  TEST(cerv_response_serialize_custom_headers_before_body);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 200);
  cerv_response_set_body(res, "hello");
  cerv_response_set_header(res, "X-Trace", "xyz");

  char *out = cerv_response_serialize(res);

  char *hdr   = strstr(out, "X-Trace");
  char *blank = strstr(out, "\r\n\r\n");
  ASSERT_NOT_NULL(hdr,   "custom header should be present");
  ASSERT_NOT_NULL(blank, "blank line should be present");
  ASSERT(hdr < blank, "custom header should appear before the blank line");

  arena_destroy(a);
}

static void test_serialize_empty_body() {
  TEST(cerv_response_serialize_empty_body);

  Arena *a = arena_new(4096);
  CervResponse *res = cerv_response_new(arena_allocator(a));
  cerv_response_set_status(res, 204);

  char *out = cerv_response_serialize(res);

  ASSERT(strstr(out, "204 No Content") != NULL, "status line for 204");
  ASSERT(strstr(out, "Content-Length: 0") != NULL, "Content-Length should be 0");
  ASSERT(strstr(out, "\r\n\r\n") != NULL, "blank line should still be present");

  arena_destroy(a);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
  typedef void (*TestFn)(void);
  struct { const char *name; TestFn fn; } tests[] = {
    {"test_response_new",                         test_response_new},
    {"test_response_set_status",                  test_response_set_status},
    {"test_response_set_body",                    test_response_set_body},
    {"test_response_set_header_single",           test_response_set_header_single},
    {"test_response_set_header_multiple",         test_response_set_header_multiple},
    {"test_serialize_status_line",                test_serialize_status_line},
    {"test_serialize_body",                       test_serialize_body},
    {"test_serialize_default_content_type",       test_serialize_default_content_type},
    {"test_serialize_custom_headers_present",     test_serialize_custom_headers_present},
    {"test_serialize_custom_headers_before_body", test_serialize_custom_headers_before_body},
    {"test_serialize_empty_body",                 test_serialize_empty_body},
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
