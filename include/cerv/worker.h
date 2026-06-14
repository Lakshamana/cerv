#include "cerv/cerv.h"
#include <stdint.h>

typedef struct {
  void (*init)    (void* self);
  void (*run)     (void* self);
  void (*teardown)(void* self);
} CervWorkerVtable;

typedef struct {
  void *self;
  int   count;
} CervWorkerConfig;

typedef struct {
  int      busy_count;
  int      idle_count;
  int      queue_depth;
  pid_t    worker_pid;
  uint64_t timestamp_ms;
} CervHeartbeat;

typedef void (*CervBootstrapFn)(Cerv* s);
typedef void (*CervTeardownFn) (Cerv* s);
