#ifndef CERV_H
#define CERV_H

#include "router.h"

typedef struct {
  CervRouter* router;
  int         port;
  int         workers;
} Cerv;

Cerv* cerv_new(int port);
int   cerv_run(Cerv *s);

#endif // CERV_H
