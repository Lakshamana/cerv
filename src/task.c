#include "cerv/task.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

Task spawn(TaskCallback callback) {
  Task t = {.fn = callback, .pid = -1, .read_fd = -1};
  int fd[2];

  if (pipe(fd) < 0) {
    perror("pipe");
    return t;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("error forking");
    close(fd[0]);
    close(fd[1]);
    return t;
  } else if (pid == 0) {
    close(fd[0]);
    callback();
    // only reached if callback failed
    write(fd[1], &errno, sizeof(errno));
    _exit(127);
  } else {
    close(fd[1]);
    t.pid = pid;
    t.read_fd = fd[0];
  }

  return t;
}

int task_wait(Task *t) {
  int status = 0;
  int child_errno = 0;

  size_t n = read(t->read_fd, &child_errno, sizeof(child_errno));
  close(t->read_fd);
  t->read_fd = -1;

  waitpid(t->pid, &status, 0);

  if (n > 0) {
    fprintf(stderr, "task failed: %s\n", strerror(child_errno));
    return 127;
  }

  return WEXITSTATUS(status);
}
