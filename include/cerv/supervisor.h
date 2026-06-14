#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stddef.h>
#include <stdint.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "cerv/chan.h"
#include "cerv/defs.h"
#include "cerv/worker.h"

// TODO: move to fibers
#define CERV_FIBER_ST__FREE     0
#define CERV_FIBER_ST__RUNNABLE 1
#define CERV_FIBER_ST__IO_WAIT  2
#define CERV_FIBER_ST__DONE     3

typedef struct {
  int      slot_index;
  int      is_resume;
  uint64_t job_id;
  size_t   len;
} CervJobPacket;

typedef struct  {
  int      slot_index;
  uint64_t job_id;
  size_t   len;
} CervResponsePacket;

typedef struct {
  int      clfd;
  int      slot_index;
  int      retry_count;
  int      active;
  pid_t    worker_pid;
  uint64_t job_id;
} CervJobEntry;

typedef struct {
  int      worker_index;
  int      alive;
  pid_t    worker_pid;
  uint64_t last_heartbeat_ms;
  Chan     work_queue;
  Chan     heartbeat;
} CervWorkerEntry;

typedef struct {
  int               alive;
  pid_t             pid;
  CervWorkerVtable *vtable;
  void             *self;
} CervUserWorkerEntry;

typedef struct {
  uint64_t current_job_id;
} CervWorkerStatus;

typedef struct {
  char       stack[CERV_DEF_FIBER_STACK_SIZE];
  int        wait_fd;
  int        state;
  uint64_t   job_id;
  ucontext_t ctx;
} CervFiberCheckpoint;

// mmap block
typedef struct {
  int                  n_slots;
  CervWorkerStatus    *worker_status;
  uint8_t             *free_list;
  uint8_t             *arena_slots;
  size_t               total_size;
  CervFiberCheckpoint *checkpoints;
  void                *base;
} CervSharedRegion;

#endif // !SUPERVISOR_H
