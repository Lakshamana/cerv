#include "cerv/router.h"

void cerv_router_init(CervRouter *r) {
  (void)r;
}

void cerv_router_add(CervRouter *r, const char *method, const char *path,
                     void *handler) {
  (void)r;
  (void)method;
  (void)path;
  (void)handler;
}

CervHandler *cerv_router_match(CervRouter *r, const char *method,
                               const char *path) {
  (void)r;
  (void)method;
  (void)path;
  return nullptr;
}
