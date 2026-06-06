#include <stddef.h>

#ifndef CERV_CHAN_H

typedef struct {
  int r_fd;
  int w_fd;
} Chan;

Chan chan_open();
int  is_chan_ok(Chan c);
int  chan_send(Chan c, const void *buf, size_t len);
int  chan_recv(Chan c, void *buf, size_t len);
int  chan_try_recv(Chan c, void *buf, size_t len);
void chan_close(Chan c);

#endif // !CERV_CHAN_H

