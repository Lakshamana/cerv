#include "../test.h"
#include "cerv/handler.h"
#include "cerv/router.h"
#include <stdio.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

typedef struct {
  CervHandler vtable; // must be first
} TestHandler;

static void test_cerv_router_new() {
  TEST(cerv_router_new);

  CervRouter *r = cerv_router_new();

  ASSERT(r->count == 0, "initial route count must be 0");
  ASSERT_NOT_NULL(r->routes, "route list should not be NULL");
}

static void test_cerv_router_add_route() {
  TEST(cerv_router_add);

  CervRouter *r = cerv_router_new();
  TestHandler *h = malloc(sizeof(TestHandler));

  int result1 = cerv_router_add(r, "POST", "/path", r);
  int result2 = cerv_router_add(r, "GET", "/another", r);

  ASSERT(result1 == 0, "should add POST route successfully");
  ASSERT(result2 == 0, "should add GET route successfully");

  ASSERT(r->count == 2, "count should be updated after adding a route");
  ASSERT_STR_EQ(r->routes[0].path, "/path", "path should be '/path'");
  ASSERT_STR_EQ(r->routes[0].method, "POST", "method should be 'POST'");

  ASSERT_STR_EQ(r->routes[1].path, "/another", "path should be '/another'");
  ASSERT_STR_EQ(r->routes[1].method, "GET", "method should be 'GET'");

  free(h);
}

static void test_cerv_router_add_route_already_exists() {
  TEST(cerv_router_add_already_exists);
  CervRouter *r = cerv_router_new();

  TestHandler *h = malloc(sizeof(TestHandler));

  // added same path, but different methods
  cerv_router_add(r, "POST", "/path", h);
  cerv_router_add(r, "GET", "/path", h);

  // try to add a route that already exists in the router
  int result = cerv_router_add(r, "GET", "/path", h);
  ASSERT_INT_EQ(result, -1, "should fail to add a duplicated route");

  free(h);
}

static void test_cerv_router_cannot_overflow_route_buffer() {
  TEST(cerv_router_cannot_overflow_route_buffer);

  CervRouter *r = cerv_router_new();
  TestHandler *h = malloc(sizeof(TestHandler));
  char name[7];
  int last_result;

  for (int i = 0; i < CERV_ROUTER_MAX_ROUTES; i++) {
    sprintf(name, "/r/%d", i);
    last_result = cerv_router_add(r, "GET", name, h);
  }

  ASSERT(last_result == -1, "it should refuse accepting routes which would cause buffer to overflow");
}

static void test_cerv_router_match() {
  TEST(cerv_router_match);

  CervRouter *r = cerv_router_new();
  CervHandler *h = (CervHandler *)malloc(sizeof(CervHandler));

  cerv_router_add(r, "POST", "/path", h);
  cerv_router_add(r, "GET", "/another", h);

  CervHandler *not_found = cerv_router_match(r, "GET", "/path");
  ASSERT_NULL(not_found, "expected not to be found");

  CervHandler *h1 = cerv_router_match(r, "GET", "/another");
  ASSERT_NOT_NULL(h1, "handler should not be NULL");
}

int main(void) {
  test_cerv_router_new();
  test_cerv_router_add_route();
  test_cerv_router_match();
  test_cerv_router_add_route_already_exists();
  test_cerv_router_cannot_overflow_route_buffer();

  printf("%s\n%d passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR,
         passed, failed);

  return failed > 0 ? 1 : 0;
}
