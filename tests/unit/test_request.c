#include "cerv/arena.h"
#include "cerv/request.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

/* ── parse_req ──────────────────────────────────────────────────────────── */

static void test_parse_get_no_body() {
  TEST(parse_get_no_body);

  Arena *a = arena_new(4096);
  const char *raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse a valid GET request");
  ASSERT_STR_EQ(req->method, "GET", "method should be GET");
  ASSERT_STR_EQ(req->path, "/hello", "path should be /hello");
  ASSERT_NULL(req->body, "body should be NULL for GET with no body");

  close_req(a);
}

static void test_parse_post_with_body() {
  TEST(parse_post_with_body);

  Arena *a = arena_new(4096);
  const char *raw = "POST /submit HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 5\r\n\r\nhello";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse a valid POST request");
  ASSERT_STR_EQ(req->method, "POST", "method should be POST");
  ASSERT_STR_EQ(req->path, "/submit", "path should be /submit");
  ASSERT_STR_EQ(req->body, "hello", "body should be 'hello'");
  ASSERT_INT_EQ((int)req->body_len, 5, "body_len should be 5");

  close_req(a);
}

static void test_parse_query_string() {
  TEST(parse_query_string);

  Arena *a = arena_new(4096);
  const char *raw = "GET /search?q=foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request with query string");
  ASSERT_STR_EQ(req->path, "/search", "path should be stripped of query string");
  ASSERT_INT_EQ((int)req->query_string_count, 1, "should have one query param");
  ASSERT_STR_EQ(req->query_string_params[0].key, "q", "param key should be q");
  ASSERT_STR_EQ(req->query_string_params[0].value, "foo", "param value should be foo");

  close_req(a);
}

static void test_parse_req_qs_multiple_params() {
  TEST(parse_req_qs_multiple_params);

  Arena *a = arena_new(4096);
  const char *raw = "GET /route?a=1&b=2 HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request with multiple query params");
  ASSERT_STR_EQ(req->path, "/route", "path should not contain query string");
  ASSERT_INT_EQ((int)req->query_string_count, 2, "should have two query params");
  ASSERT_STR_EQ(req->query_string_params[0].key, "a", "first key should be a");
  ASSERT_STR_EQ(req->query_string_params[0].value, "1", "first value should be 1");
  ASSERT_STR_EQ(req->query_string_params[1].key, "b", "second key should be b");
  ASSERT_STR_EQ(req->query_string_params[1].value, "2", "second value should be 2");

  const char *val = qs_get(req->query_string_params, "b");
  ASSERT_STR_EQ(val, "2", "qs_get should find param populated by parse_req");

  close_req(a);
}

static void test_parse_req_malformed_qs() {
  TEST(parse_req_malformed_qs);

  Arena *a = arena_new(4096);
  const char *raw = "GET /path?keyonly HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NULL(req, "malformed query string should return NULL from parse_req");

  close_req(a);
}

static void test_parse_multi_segment_path() {
  TEST(parse_multi_segment_path);

  Arena *a = arena_new(4096);
  const char *raw = "GET /api/v1/users HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request with multi-segment path");
  ASSERT_STR_EQ(req->path, "/api/v1/users", "path should be /api/v1/users");

  close_req(a);
}

static void test_parse_large_url() {
  TEST(parse_large_url);

#define LARGE_URL_LEN 8192
  char url[LARGE_URL_LEN + 1];
  memset(url, 'a', LARGE_URL_LEN);
  url[LARGE_URL_LEN] = '\0';

  char *raw = malloc(LARGE_URL_LEN + 64);
  int len = snprintf(raw, LARGE_URL_LEN + 64,
                     "GET /%s HTTP/1.1\r\nHost: localhost\r\n\r\n", url);

  Arena *a = arena_new(4096);
  CervRequest *req = parse_req(raw, len, arena_allocator(a));
  ASSERT_NOT_NULL(req, "should parse request with large URL");
  ASSERT_INT_EQ((int)req->path_len, LARGE_URL_LEN + 1, "path_len should match");

  char *expected = malloc(LARGE_URL_LEN + 2);
  snprintf(expected, LARGE_URL_LEN + 2, "/%s", url);
  ASSERT_STR_EQ(req->path, expected, "large path should be fully preserved");

  close_req(a);
  free(raw);
  free(expected);
#undef LARGE_URL_LEN
}

static void test_parse_large_body() {
  TEST(parse_large_body);

#define LARGE_BODY_LEN 16384
  char *body = malloc(LARGE_BODY_LEN + 1);
  memset(body, 'x', LARGE_BODY_LEN);
  body[LARGE_BODY_LEN] = '\0';

  char *raw = malloc(LARGE_BODY_LEN + 128);
  int len = snprintf(raw, LARGE_BODY_LEN + 128,
                     "POST /upload HTTP/1.1\r\nHost: localhost\r\n"
                     "Content-Length: %d\r\n\r\n%s",
                     LARGE_BODY_LEN, body);

  Arena *a = arena_new(4096);
  CervRequest *req = parse_req(raw, len, arena_allocator(a));
  ASSERT_NOT_NULL(req, "should parse request with large body");
  ASSERT_INT_EQ((int)req->body_len, LARGE_BODY_LEN, "body_len should match");
  ASSERT_STR_EQ(req->body, body, "large body should be fully preserved");

  close_req(a);
  free(body);
  free(raw);
#undef LARGE_BODY_LEN
}

static void test_parse_method_variants() {
  TEST(parse_method_variants);

  struct {
    const char *raw;
    const char *method;
  } cases[] = {
      {"PUT /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
       "PUT"},
      {"DELETE /resource HTTP/1.1\r\nHost: localhost\r\n\r\n", "DELETE"},
      {"PATCH /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
       "0\r\n\r\n",
       "PATCH"},
  };

  for (int i = 0; i < 3; i++) {
    Arena *a = arena_new(4096);
    CervRequest *req = parse_req(cases[i].raw, strlen(cases[i].raw), arena_allocator(a));
    ASSERT_NOT_NULL(req, "should parse request");
    ASSERT_STR_EQ(req->method, cases[i].method, "should match method");
    close_req(a);
  }
}

static void test_parse_malformed() {
  TEST(parse_malformed);

  Arena *a = arena_new(4096);
  const char *raw = "!nvalid request\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NULL(req, "should return NULL for malformed request");

  close_req(a);
}

static void test_parse_truncated() {
  TEST(parse_truncated);

  Arena *a = arena_new(4096);
  const char *raw = "GET / HTTP/1.1\r\nHost: local";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "truncated request returns partial result, not NULL");
  ASSERT_STR_EQ(req->path, "/", "path should be parsed before truncation");

  close_req(a);
}

static void test_parse_content_length_mismatch() {
  TEST(parse_content_length_mismatch);

  Arena *a = arena_new(4096);
  const char *raw = "POST / HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 100\r\n\r\nshortbody";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should not crash on content-length mismatch");
  ASSERT_INT_EQ((int)req->body_len, 9,
                "should store only the received body bytes");

  close_req(a);
}

static void test_parse_root_path() {
  TEST(parse_root_path);

  Arena *a = arena_new(4096);
  const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request with root path");
  ASSERT_STR_EQ(req->path, "/", "path should be /");

  close_req(a);
}

static void test_parse_post_empty_body() {
  TEST(parse_post_empty_body);

  Arena *a = arena_new(4096);
  const char *raw = "POST /submit HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 0\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse POST with Content-Length: 0");
  ASSERT_NULL(req->body, "body should be NULL when Content-Length is 0");
  ASSERT_INT_EQ((int)req->body_len, 0, "body_len should be 0");

  close_req(a);
}

/* ── parse_qs / qs_get ───────────────────────────────────────────────────── */

static void test_qs_single_param() {
  TEST(qs_single_param);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/search?q=hello");

  ASSERT_INT_EQ(count, 1, "should parse one param");
  ASSERT_STR_EQ(params[0].key, "q", "key should be q");
  ASSERT_STR_EQ(params[0].value, "hello", "value should be hello");
}

static void test_qs_multiple_params() {
  TEST(qs_multiple_params);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/path?a=1&b=2&c=3");

  ASSERT_INT_EQ(count, 3, "should parse three params");
  ASSERT_STR_EQ(params[0].key, "a", "first key should be a");
  ASSERT_STR_EQ(params[0].value, "1", "first value should be 1");
  ASSERT_STR_EQ(params[1].key, "b", "second key should be b");
  ASSERT_STR_EQ(params[1].value, "2", "second value should be 2");
  ASSERT_STR_EQ(params[2].key, "c", "third key should be c");
  ASSERT_STR_EQ(params[2].value, "3", "third value should be 3");
}

static void test_qs_no_query_string() {
  TEST(qs_no_query_string);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/hello");

  ASSERT_INT_EQ(count, 0, "no query string should return 0");
}

static void test_qs_empty_after_question_mark() {
  TEST(qs_empty_after_question_mark);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/hello?");

  ASSERT_INT_EQ(count, 0, "bare ? should return 0");
}

static void test_qs_empty_value() {
  TEST(qs_empty_value);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/path?key=");

  ASSERT_INT_EQ(count, 1, "should parse param with empty value");
  ASSERT_STR_EQ(params[0].key, "key", "key should be key");
  ASSERT_STR_EQ(params[0].value, "", "value should be empty string");
}

static void test_qs_malformed_no_equals() {
  TEST(qs_malformed_no_equals);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/path?keyonly");

  ASSERT_INT_EQ(count, -1, "key without = should return -1");
}

static void test_qs_malformed_empty_key() {
  TEST(qs_malformed_empty_key);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, "/path?=value");

  ASSERT_INT_EQ(count, -1, "empty key should return -1");
}

static void test_qs_cap_at_max_params() {
  TEST(qs_cap_at_max_params);

  char qs[512] = "/path?";
  for (int i = 0; i < 20; i++) {
    char pair[16];
    snprintf(pair, sizeof(pair), "k%d=%d%s", i, i, i < 19 ? "&" : "");
    strncat(qs, pair, sizeof(qs) - strlen(qs) - 1);
  }

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  int count = parse_qs(params, qs);

  ASSERT_INT_EQ(count, CERV_MAX_REQ_PARAMS, "should cap at CERV_MAX_REQ_PARAMS");
}

static void test_qs_get_found() {
  TEST(qs_get_found);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  parse_qs(params, "/path?foo=bar&baz=qux");

  const char *val = qs_get(params, "baz");
  ASSERT_STR_EQ(val, "qux", "qs_get should return value for existing key");
}

static void test_qs_get_not_found() {
  TEST(qs_get_not_found);

  RequestParam params[CERV_MAX_REQ_PARAMS] = {0};
  parse_qs(params, "/path?foo=bar");

  const char *val = qs_get(params, "missing");
  ASSERT_NULL(val, "qs_get should return NULL for missing key");
}

/* ── headers ─────────────────────────────────────────────────────────────── */

static void test_headers_single() {
  TEST(headers_single);

  Arena *a = arena_new(4096);
  const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request");
  ASSERT_INT_EQ((int)req->headers_count, 1, "should have one header");
  ASSERT_STR_EQ(req->headers[0].key, "Host", "header key should be Host");
  ASSERT_STR_EQ(req->headers[0].value, "localhost", "header value should be localhost");

  close_req(a);
}

static void test_headers_multiple() {
  TEST(headers_multiple);

  Arena *a = arena_new(4096);
  const char *raw = "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Accept: text/plain\r\n"
                    "Connection: close\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse request");
  ASSERT_INT_EQ((int)req->headers_count, 3, "should have three headers");
  ASSERT_STR_EQ(req->headers[1].key, "Accept", "second key should be Accept");
  ASSERT_STR_EQ(req->headers[1].value, "text/plain", "second value should be text/plain");

  close_req(a);
}

static void test_headers_post_with_content_type() {
  TEST(headers_post_with_content_type);

  Arena *a = arena_new(4096);
  const char *raw = "POST /submit HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: 2\r\n\r\n{}";
  CervRequest *req = parse_req(raw, strlen(raw), arena_allocator(a));

  ASSERT_NOT_NULL(req, "should parse POST with content-type header");
  ASSERT_INT_EQ((int)req->headers_count, 3, "should have three headers");
  ASSERT_STR_EQ(req->headers[1].key, "Content-Type", "key should be Content-Type");
  ASSERT_STR_EQ(req->headers[1].value, "application/json", "value should be application/json");

  close_req(a);
}

/* ── check_done ─────────────────────────────────────────────────────────── */

static void test_check_done_complete() {
  TEST(check_done_complete);

  int done = 0;
  const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  check_done(raw, strlen(raw), &done);

  ASSERT_INT_EQ(done, 1, "complete request should set done to 1");
}

static void test_check_done_partial() {
  TEST(check_done_partial);

  int done = 0;
  const char *raw = "GET / HTTP/1.1\r\nHost: local";
  check_done(raw, strlen(raw), &done);

  ASSERT_INT_EQ(done, 0, "partial request should leave done as 0");
}

static void test_check_done_malformed() {
  TEST(check_done_malformed);

  int done = 0;
  const char *raw = "!nvalid request\r\n\r\n";
  check_done(raw, strlen(raw), &done);

  ASSERT_INT_EQ(done, -1, "malformed request should set done to -1");
}

int main(int argc, char *argv[]) {
  typedef void (*TestFn)(void);
  struct { const char *name; TestFn fn; } tests[] = {
    {"test_parse_get_no_body",             test_parse_get_no_body},
    {"test_parse_post_with_body",          test_parse_post_with_body},
    {"test_parse_query_string",            test_parse_query_string},
    {"test_parse_req_qs_multiple_params",  test_parse_req_qs_multiple_params},
    {"test_parse_req_malformed_qs",        test_parse_req_malformed_qs},
    {"test_parse_multi_segment_path",      test_parse_multi_segment_path},
    {"test_parse_large_url",               test_parse_large_url},
    {"test_parse_large_body",              test_parse_large_body},
    {"test_parse_method_variants",         test_parse_method_variants},
    {"test_parse_malformed",               test_parse_malformed},
    {"test_parse_truncated",               test_parse_truncated},
    {"test_parse_content_length_mismatch", test_parse_content_length_mismatch},
    {"test_parse_root_path",               test_parse_root_path},
    {"test_parse_post_empty_body",         test_parse_post_empty_body},
    {"test_qs_single_param",               test_qs_single_param},
    {"test_qs_multiple_params",            test_qs_multiple_params},
    {"test_qs_no_query_string",            test_qs_no_query_string},
    {"test_qs_empty_after_question_mark",  test_qs_empty_after_question_mark},
    {"test_qs_empty_value",                test_qs_empty_value},
    {"test_qs_malformed_no_equals",        test_qs_malformed_no_equals},
    {"test_qs_malformed_empty_key",        test_qs_malformed_empty_key},
    {"test_qs_cap_at_max_params",          test_qs_cap_at_max_params},
    {"test_qs_get_found",                  test_qs_get_found},
    {"test_qs_get_not_found",              test_qs_get_not_found},
    {"test_headers_single",                test_headers_single},
    {"test_headers_multiple",              test_headers_multiple},
    {"test_headers_post_with_content_type", test_headers_post_with_content_type},
    {"test_check_done_complete",           test_check_done_complete},
    {"test_check_done_partial",            test_check_done_partial},
    {"test_check_done_malformed",          test_check_done_malformed},
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
