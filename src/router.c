#include "cerv/router.h"
#include <stdlib.h>
#include <string.h>

CervRouter *cerv_router_new() {
  CervRouter *r = (CervRouter *)malloc(sizeof(CervRouter));
  r->count = 0;
  return r;
}

void cerv_router_add(CervRouter *r, const char *method, const char *path,
                     void *handler) {
  int count = r->count;
  CervRoute route = {.method = method, .path = path, .handler = handler};
  r->routes[count++] = route;
  r->count = count;
}

CervHandler *cerv_router_match(CervRouter *r, const char *method,
                               const char *path) {
  for (int i = 0; i < r->count; i++) {
    CervRoute route = r->routes[i];
    if (strcmp(route.method, method) && strcmp(route.path, path)) {
      return route.handler;
    }
  }

  return NULL;
}
