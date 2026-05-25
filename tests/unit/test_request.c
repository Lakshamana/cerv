#include "cerv/request.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

/* ── parse_req ──────────────────────────────────────────────────────────── */

static void test_parse_get_no_body() {
  TEST(parse_get_no_body);

  const char *raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse a valid GET request");
  ASSERT_STR_EQ(req->method, "GET", "method should be GET");
  ASSERT_STR_EQ(req->path, "/hello", "path should be /hello");
  ASSERT_NULL(req->body, "body should be NULL for GET with no body");

  close_req(req);
}

static void test_parse_post_with_body() {
  TEST(parse_post_with_body);

  const char *raw = "POST /submit HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 5\r\n\r\nhello";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse a valid POST request");
  ASSERT_STR_EQ(req->method, "POST", "method should be POST");
  ASSERT_STR_EQ(req->path, "/submit", "path should be /submit");
  ASSERT_STR_EQ(req->body, "hello", "body should be 'hello'");
  ASSERT_INT_EQ((int)req->body_len, 5, "body_len should be 5");

  close_req(req);
}

static void test_parse_query_string() {
  TEST(parse_query_string);

  const char *raw = "GET /search?q=foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse request with query string");
  ASSERT_STR_EQ(req->path, "/search?q=foo", "path should include query string");

  close_req(req);
}

static void test_parse_multi_segment_path() {
  TEST(parse_multi_segment_path);

  const char *raw = "GET /api/v1/users HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse request with multi-segment path");
  ASSERT_STR_EQ(req->path, "/api/v1/users", "path should be /api/v1/users");

  close_req(req);
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

  CervRequest *req = parse_req(raw, len);
  ASSERT_NOT_NULL(req, "should parse request with large URL");
  ASSERT_INT_EQ((int)req->path_len, LARGE_URL_LEN + 1, "path_len should match");

  char *expected = malloc(LARGE_URL_LEN + 2);
  snprintf(expected, LARGE_URL_LEN + 2, "/%s", url);
  ASSERT_STR_EQ(req->path, expected, "large path should be fully preserved");

  close_req(req);
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

  CervRequest *req = parse_req(raw, len);
  ASSERT_NOT_NULL(req, "should parse request with large body");
  ASSERT_INT_EQ((int)req->body_len, LARGE_BODY_LEN, "body_len should match");
  ASSERT_STR_EQ(req->body, body, "large body should be fully preserved");

  close_req(req);
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
    CervRequest *req = parse_req(cases[i].raw, strlen(cases[i].raw));
    ASSERT_NOT_NULL(req, "should parse request");
    ASSERT_STR_EQ(req->method, cases[i].method, "should match method");
    close_req(req);
  }
}

static void test_parse_malformed() {
  TEST(parse_malformed);

  const char *raw = "!nvalid request\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NULL(req, "should return NULL for malformed request");
}

static void test_parse_truncated() {
  TEST(parse_truncated);

  // truncated before \r\n\r\n: llhttp returns HPE_OK (incomplete, not an
  // error), so parse_req returns a partial CervRequest — the key is it doesn't
  // crash
  const char *raw = "GET / HTTP/1.1\r\nHost: local";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "truncated request returns partial result, not NULL");
  ASSERT_STR_EQ(req->path, "/", "path should be parsed before truncation");

  close_req(req);
}

static void test_parse_content_length_mismatch() {
  TEST(parse_content_length_mismatch);

  // CL claims 100 bytes but body is only 9 — no crash, partial body stored
  const char *raw = "POST / HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 100\r\n\r\nshortbody";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should not crash on content-length mismatch");
  ASSERT_INT_EQ((int)req->body_len, 9,
                "should store only the received body bytes");

  close_req(req);
}

static void test_parse_root_path() {
  TEST(parse_root_path);

  const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse request with root path");
  ASSERT_STR_EQ(req->path, "/", "path should be /");

  close_req(req);
}

static void test_parse_post_empty_body() {
  TEST(parse_post_empty_body);

  const char *raw = "POST /submit HTTP/1.1\r\nHost: localhost\r\n"
                    "Content-Length: 0\r\n\r\n";
  CervRequest *req = parse_req(raw, strlen(raw));

  ASSERT_NOT_NULL(req, "should parse POST with Content-Length: 0");
  ASSERT_NULL(req->body, "body should be NULL when Content-Length is 0");
  ASSERT_INT_EQ((int)req->body_len, 0, "body_len should be 0");

  close_req(req);
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

int main(void) {
  test_parse_get_no_body();
  test_parse_post_with_body();
  test_parse_query_string();
  test_parse_multi_segment_path();
  test_parse_large_url();
  test_parse_large_body();
  test_parse_method_variants();
  test_parse_malformed();
  test_parse_truncated();
  test_parse_content_length_mismatch();
  test_parse_root_path();
  test_parse_post_empty_body();
  test_check_done_complete();
  test_check_done_partial();
  test_check_done_malformed();

  printf("%s\n%d passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR,
         passed, failed);

  return failed > 0 ? 1 : 0;
}
