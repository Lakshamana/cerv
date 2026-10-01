#ifndef CERV_H
#define CERV_H

#include "cerv/defs.h"
#include "cerv/supervisor.h"
#include "router.h"

typedef struct {
  CervRouter*          router;
  int                  port;
  int                  workers;
  void*                deps;
  CervWorkerEntry      internal_workers[CERV_DEF_MAX_WORKERS];
  CervUserWorkerEntry  user_workers[CERV_DEF_MAX_WORKERS];
  CervSharedRegion    *s_region;
  Chan                 response_pipe;
  Chan                 sig_pipe;
} Cerv;

typedef void (*CervBootstrapFn)(Cerv* s);
typedef void (*CervTeardownFn) (Cerv* s);

Cerv* cerv_new(int port);
int   cerv_run(Cerv *s, CervBootstrapFn bootstrap, CervTeardownFn teardown);
void  cerv_worker_main(Cerv* c, int w_idx);


#endif // CERV_H
