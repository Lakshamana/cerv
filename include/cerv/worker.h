#ifndef CERV_WORKER_H
#define CERV_WORKER_H

#include <stdint.h>
#include <unistd.h>

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

#endif // !CERV_WORKER_H
