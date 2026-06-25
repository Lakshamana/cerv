#include "cerv/process.h"
#include "cerv/supervisor.h"
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

Process spawn(void *ctx, SpawnFn callback) {
  Process p = {.pid = -1, .stop_w_fd = -1};

  pid_t pid = fork();
  if (pid < 0) {
    return (Process){.pid = -1, .stop_w_fd = -1};
  } else if (pid == 0) {
    callback(ctx);
    _exit(0);
  }

  p.pid = pid;
  return p;
}

Process supervise(void *ctx, SuperviseFn callback) {
  Chan ready_chan = chan_open(); // child signals parent ready
  if (!is_chan_ok(ready_chan)) {
    return (Process){.pid = -1, .stop_w_fd = -1};
  }

  Chan stop_chan = chan_open(); // parent signals child to stop
  if (!is_chan_ok(stop_chan)) {
    chan_close(ready_chan);
    return (Process){.pid = -1, .stop_w_fd = -1};
  }

  Process p = {.pid = -1, .stop_w_fd = -1};

  pid_t pid = fork();
  if (pid < 0) {
    chan_close(ready_chan);
    chan_close(stop_chan);
    return p;
  } else if (pid == 0) {
    close(ready_chan.r_fd);
    close(stop_chan.w_fd);

    // stop read end will be passed, so children can be notified about signals
    callback(ctx, &ready_chan, stop_chan.r_fd);
    _exit(0);
  }

  close(ready_chan.w_fd);
  close(stop_chan.r_fd);

  // block on children to write ready_byte after `init()` hook
  uint8_t ready_byte = 0;
  chan_recv(ready_chan, &ready_byte, sizeof(ready_byte));

  // no longer needed
  chan_close(ready_chan);

  p.pid = pid;
  p.stop_w_fd = stop_chan.w_fd;

  return p;
}
