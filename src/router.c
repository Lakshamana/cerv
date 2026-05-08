#include "cerv/router.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

CervRouter *cerv_router_new() {
  CervRouter *r = (CervRouter *)malloc(sizeof(CervRouter));
  r->count = 0;
  return r;
}

int cerv_router_add(CervRouter *r, const char *method, const char *path,
                    void *handler) {
  int count = r->count;
  assert(count < CERV_ROUTER_MAX_ROUTES);
  assert(handler != NULL);

  if (count == CERV_ROUTER_MAX_ROUTES) {
    return -1;
  }

  // check whether route already exists
  void *match = cerv_router_match(r, method, path);
  if (match != NULL) {
    return -1;
  }

  CervRoute route = {.method = method, .path = path, .handler = handler};
  r->routes[count++] = route;
  r->count = count;

  return 0;
}

CervHandler *cerv_router_match(CervRouter *r, const char *method,
                               const char *path) {
  for (int i = 0; i < r->count; i++) {
    CervRoute route = r->routes[i];
    if (strcmp(route.method, method) == 0 && strcmp(route.path, path) == 0) {
      return route.handler;
    }
  }

  return NULL;
}
