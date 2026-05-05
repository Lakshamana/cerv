#ifndef CERV_H
#define CERV_H

#include "handler.h"
#include "request.h"
#include "response.h"
#include "router.h"
#include "task.h"

typedef struct {
  CervRouter router;
  int        port;
  int        max_workers; // pre-forked worker pool size
} CervServer;

CervServer* cerv_new(int port, int max_workers);
int         cerv_run(CervServer *s);

#endif // CERV_H
