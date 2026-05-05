#include "cerv/cerv.h"
#include <stdlib.h>

CervServer *cerv_new(int port, int max_workers) {
  CervServer *s = malloc(sizeof(CervServer));
  s->max_workers = max_workers;
  s->port = port;

  return s;
}

int cerv_run(CervServer *s) {
  (void)s;
  return 0;
}
