#ifndef CERV_PROCESS_H
#define CERV_PROCESS_H

#include "cerv/supervisor.h"
#include <unistd.h>

typedef void (*SuperviseFn)(void *ctx, Chan *ready, int stop_fd);
typedef void (*SpawnFn)(void *ctx);

typedef struct {
  pid_t     pid;
  int       stop_w_fd;
} Process;

/**
 * Forks a child process and runs callback() in it.
 * A pipe is used to detect exec failures — if callback() returns,
 * errno is written to the pipe and the child exits 127.
 */
Process spawn(void* ctx, SpawnFn callback);

/**
 * Returns a supervised Process
 */
Process supervise(void* ctx, SuperviseFn callback);

#endif // CERV_PROCESS_H
