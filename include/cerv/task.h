#ifndef CERV_TASK_H
#define CERV_TASK_H

#include <unistd.h>

typedef int (*TaskCallback)();

typedef struct {
  pid_t pid;
  TaskCallback fn;
  int read_fd;
} Task;

/**
 * Forks a child process and runs callback() in it.
 * A pipe is used to detect exec failures — if callback() returns,
 * errno is written to the pipe and the child exits 127.
 */
Task spawn(TaskCallback callback);

/**
 * Waits for the task to finish.
 * Returns the exit status, or 127 if the callback failed.
 */
int task_wait(Task *t);

#endif // CERV_TASK_H
