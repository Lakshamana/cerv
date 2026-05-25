#include "cerv/router.h"
#include "cerv/defs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

CervRouter *cerv_router_new() {
  CervRouter *r = (CervRouter *)calloc(1, sizeof(CervRouter));
  r->count = 0;
  return r;
}

int cerv_router_add(CervRouter *r, const char *method, const char *path,
                    void *handler) {
  int count = r->count;
  assert(count < CERV_ROUTER_MAX_ROUTES);
  assert(handler != NULL);

  if (count == CERV_ROUTER_MAX_ROUTES) {
    return CERV_DEF_RET_ERROR;
  }

  // check whether route already exists
  void *match = cerv_router_match(r, method, path);
  if (match != NULL) {
    return CERV_DEF_RET_ERROR;
  }

  CervRoute route = {.method = method, .path = path, .handler = handler};
  r->routes[count++] = route;
  r->count = count;

  return CERV_DEF_RET_OK;
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
