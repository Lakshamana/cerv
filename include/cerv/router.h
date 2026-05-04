#ifndef CERV_ROUTER_H
#define CERV_ROUTER_H

#include "handler.h"

#define CERV_ROUTER_MAX_ROUTES 64

typedef struct {
  const char  *method;
  const char  *path;
  CervHandler *handler;
} CervRoute;

typedef struct {
  CervRoute routes[CERV_ROUTER_MAX_ROUTES];
  int       count;
} CervRouter;

void         cerv_router_init(CervRouter *r);
void         cerv_router_add(CervRouter *r, const char *method, const char *path, void *handler);
CervHandler *cerv_router_match(CervRouter *r, const char *method, const char *path);

#endif // CERV_ROUTER_H
