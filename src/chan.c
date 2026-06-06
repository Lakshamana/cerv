#include <asm-generic/errno-base.h>
#include <cerv/chan.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>

Chan chan_open() {
  int pipedes[2];

  if (pipe(pipedes) < 0) {
    return (Chan){-1, -1};
  }

  return (Chan){pipedes[0], pipedes[1]};
}

int is_chan_ok(Chan c) { return c.r_fd != -1 && c.w_fd != -1; }

int chan_send(Chan c, const void *buf, size_t len) {
  int res;
  while ((res = write(c.w_fd, buf, len)) < 0 && errno == EINTR) {
  }

  return res;
}

int chan_recv(Chan c, void *buf, size_t len) {
  int res;
  while ((res = read(c.r_fd, buf, len)) < 0 && errno == EINTR) {
  }

  return res;
}

int chan_try_recv(Chan c, void *buf, size_t len) {
  int p_res;
  struct pollfd pfd = {0};
  pfd.fd = c.r_fd;
  pfd.events = POLLIN;

  if ((p_res = poll(&pfd, 1, 0)) < 1) {
    errno = p_res < 0 ? errno : EAGAIN;
    return -1;
  }

  return read(c.r_fd, buf, len);
}

void chan_close(Chan c) {
  close(c.r_fd);
  close(c.w_fd);
}
