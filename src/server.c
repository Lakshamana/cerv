#include "cerv/arena.h"
#include "cerv/cerv.h"
#include "cerv/defs.h"
#include "cerv/handler.h"
#include "cerv/process.h"
#include "cerv/request.h"
#include "cerv/response.h"
#include "cerv/router.h"
#include "cerv/supervisor.h"
#include <asm-generic/errno-base.h>
#include <bits/types/struct_timeval.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define EXITONERROR(msg, result, socket_d)                                     \
  do {                                                                         \
    const char *_m = (msg);                                                    \
    struct addrinfo *_r = (result);                                            \
    int _sd = (socket_d);                                                      \
                                                                               \
    fprintf(stderr, _m, errno);                                                \
    freeaddrinfo(_r);                                                          \
    close(_sd);                                                                \
    return CERV_DEF_RET_ERROR;                                                 \
  } while (0)

typedef struct {
  Cerv *s;
  int worker_index;
} CervWorkerCtx;


// statics
static int sig_pipe_w_fd = -1;

static void cerv_worker_cb(void *ctx) {
  CervWorkerCtx *c = (CervWorkerCtx *)ctx;
  cerv_worker_main(c->s, c->worker_index);
}

Cerv *cerv_new(int port) {
  Cerv *s = calloc(1, sizeof(Cerv));
  CervRouter *r = cerv_router_new();
  s->workers = CERV_DEF_MIN_WORKERS;
  s->port = port;
  s->router = r;

  for (int i = 0; i < CERV_DEF_MAX_WORKERS; i++)
    s->user_workers[i].stop_w_fd = -1;

  return s;
}

static CervSharedRegion *supervisor_shm_alloc(int n_workers) {
  size_t n_slots = n_workers * CERV_DEF_MAX_FIBERS_PER_WORKER;
  size_t size = sizeof(CervSharedRegion) +
                n_workers * sizeof(CervWorkerStatus) + n_slots +
                n_slots * CERV_DEF_ARENA_SLOT_SIZE +
                n_slots * sizeof(CervFiberCheckpoint);

  void *base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  CervSharedRegion *r = (CervSharedRegion *)base;
  uint8_t *cursor = (uint8_t *)base + sizeof(CervSharedRegion);

  // set base and size headers
  r->base = base;
  r->n_slots = n_slots;
  r->total_size = size;

  // set worker status list
  r->worker_status = (CervWorkerStatus *)cursor;
  cursor += n_workers * sizeof(CervWorkerStatus);

  // set free slots bitset
  r->free_list = cursor;
  cursor += n_slots;

  // set checkpoints
  r->checkpoints = (CervFiberCheckpoint *)cursor;
  cursor += n_slots * sizeof(CervFiberCheckpoint);

  r->arena_slots = cursor;

  return r;
}

static void supervisor_shm_free(CervSharedRegion *region) {
  munmap(region, region->total_size);
}

static int supervisor_fork_cerv_worker(Cerv *s, int worker_index) {
  CervWorkerEntry *entry = &s->internal_workers[worker_index];
  entry->worker_index = worker_index;

  Chan heartbeat = chan_open();
  if (!is_chan_ok(heartbeat)) {
    return CERV_DEF_RET_ERROR;
  }

  Chan queue = chan_open();
  if (!is_chan_ok(queue)) {
    chan_close(heartbeat);
    return CERV_DEF_RET_ERROR;
  }

  entry->work_queue = queue;
  entry->heartbeat = heartbeat;
  entry->last_heartbeat_ms = 0;
  entry->alive = 1;

  Process spawned = spawn(
      &(CervWorkerCtx){.s = s, .worker_index = worker_index}, cerv_worker_cb);
  if (!is_process_ok(&spawned)) {
    chan_close(heartbeat);
    chan_close(queue);
    return CERV_DEF_RET_ERROR;
  }

  entry->worker_pid = spawned.pid;

  return CERV_DEF_RET_OK;
}

static int get_max_fd(int *fds, ...) {
  va_list args;
  va_start(args, fds);

  int max = *fds;
  int *fd;

  while ((fd = va_arg(args, int *)) != NULL) {
    if (*fd > max)
      max = *fd;
  }

  va_end(args);
  return max;
}

static int get_max_heartbeat_fd(Cerv *s) {
  int max_hb_fd = s->internal_workers[0].heartbeat.r_fd;
  for (int i = 1; i < s->workers; i++) {
    int hb = s->internal_workers[i].heartbeat.r_fd;
    if (hb > max_hb_fd)
      max_hb_fd = hb;
  }

  return max_hb_fd;
}

static void supervisor_sigchld_handler(int sig) {
  write(sig_pipe_w_fd, &(uint8_t){1}, 1);
}

/*
 * Pre-fork supervisor. This will fork() `s->workers` worth stateless to process
incoming requests.
 *
 * The bootstrap flow consists of a few steps:
 * 1. socket setup flow (getaddrinfo, socket, bind, listen)
 * 2. shared memory allocation: this creates a shared memory region, where all
the forked workers
 * share the same physical pages as the parent supervisor process
 * 3. Call user-defined bootstrap function: at this point no workers where
forked yet,
 * so use this as a hook to define file descriptors to whatever external data
 * source you need. For example, you may define a struct to be passed via
s->deps (void*):
 *
   // 1. Concrete deps struct
   typedef struct {
       Conn *db;
   } MyDeps;

   // 2. Handler that casts deps and uses it
   typedef struct {
       CervHandler vtable;
   } UsersHandler;

   void users_handle(void *self, CervRequest *req, CervResponse *res, void
*deps) { MyDeps *d = deps;

       DBResult *r = DBexec(d->db, "SELECT count(*) FROM users");
       // ... build response from r ...
       DBclear(r);

       cerv_response_set_status(res, 200);
       cerv_response_set_body(res, "ok");
   }

   static UsersHandler users_h = { .vtable = { users_handle, NULL } };

   // 3. Bootstrap: called before workers are forked
   void my_bootstrap(Cerv *s) {
       MyDeps *d = calloc(1, sizeof(MyDeps));
       d->db = connectdb("host=localhost dbname=myapp");
       s->deps = d;   // workers inherit this via fork()
   }

   // 4. Teardown: called after all workers exit
   void my_teardown(Cerv *s) {
       MyDeps *d = s->deps;
       DBfinish(d->db);
       free(d);
   }
 *
 * the idea behind this is that `MyDeps` is opaque to Cerv, it doesn't need to
know
 * the specific type, but user-defined handlers should know how to `(void*)
deps`
 * into a concrete struct.
 */
int cerv_run(Cerv *s, CervBootstrapFn bootstrap, CervTeardownFn teardown) {
  // 1. socket setup
  int status, socket_d;
  struct addrinfo hint;
  struct addrinfo *result;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", s->port);
  if ((status = getaddrinfo(NULL, port_str, &hint, &result)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return CERV_DEF_RET_ERROR;
  }

  socket_d =
      socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (socket_d == -1) {
    EXITONERROR("error allocating socket: %d", result, socket_d);
  }

  int opt = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if ((bind(socket_d, result->ai_addr, result->ai_addrlen)) != 0) {
    EXITONERROR("error binding to socket err: %d\n", result, socket_d);
  }

  if (listen(socket_d, CERV_DEF_MAX_BACKLOG) == -1) {
    fprintf(stderr, "error listening to the port=%d: %d", s->port, errno);
    return CERV_DEF_RET_ERROR;
  }

  // 2. allocate shared memory
  supervisor_shm_alloc(s->workers);

  // 3. call bootstrap hook
  bootstrap(s);

  // 4. fork cerv-space workers
  for (int i = 0; i < s->workers; i++) {
    supervisor_fork_cerv_worker(s, i);
  }

  // 5. Create the response channel
  s->response_pipe = chan_open();
  CervResponsePacket response_packet = {0};
  if (!is_chan_ok(s->response_pipe)) {
    EXITONERROR("error opening response chan, err: %d\n", result, socket_d);
  }

  // 6. setup self-pipe SIGCHLD
  struct sigaction act;
  s->sig_pipe = chan_open();
  if (!is_chan_ok(s->sig_pipe)) {
    EXITONERROR("error opening sig_pipe chan, err: %d\n", result, socket_d);
  }

  act.sa_restorer = NULL;
  act.sa_handler = supervisor_sigchld_handler;
  sigaction(SIGCHLD, &act, NULL);

  // 7. Supervisor event loop
  fd_set rfds;
  while (1) {
    // define fd sets
    FD_ZERO(&rfds);
    FD_SET(socket_d, &rfds);
    FD_SET(s->response_pipe.r_fd, &rfds);
    FD_SET(s->sig_pipe.r_fd, &rfds);
    for (int i = 0; i < s->workers; i++) {
      FD_SET(s->internal_workers[i].heartbeat.r_fd, &rfds);
    }

    // get MAX nfds bit
    int max_hearbeats = get_max_heartbeat_fd(s);
    int nfds = get_max_fd(&socket_d, &s->response_pipe.r_fd, &s->sig_pipe.r_fd, &max_hearbeats, NULL) + 1;

    // select
    struct timeval timeout = {.tv_sec = 2};
    int n = select(nfds, &rfds, NULL, NULL, &timeout);
    if (n < 0) {
      // if no chan yielded, go on
      if (errno == EINTR) {
        continue;
      } else {
        // teardown
        break;
      }
    } if (n == 0) {
      // TODO: check heartbeat staleness here
    } else {
      // 7.1 check readable socket_d
      if (FD_ISSET(socket_d, &rfds)) {
        // TODO: call supervisor_accept_and_enqueue()
      }

      if (FD_ISSET(s->sig_pipe.r_fd, &rfds)) {
        // TODO: call supervisor_reap_and_recover()
      }
    }
  }

  // TODO: move this to worker main function
  while (1) {
    int clfd;
    if ((clfd = accept(socket_d, NULL, NULL)) < 0) {
      fprintf(stderr, "could not accept request: %d", errno);
    }

    int n_bytes;
    char *reqbuf = calloc(1, CERV_DEF_READ_CHUNK);
    int done = 0;
    size_t buf_s = 0;
    while ((n_bytes = recv(clfd, reqbuf + buf_s, CERV_DEF_READ_CHUNK, 0)) > 0) {
      buf_s += n_bytes;
      check_done(reqbuf, buf_s, &done);

      if (done)
        break;

      reqbuf = realloc(reqbuf, buf_s + CERV_DEF_READ_CHUNK);
    }

    Arena *arena = arena_new(CERV_DEF_ARENA_DEFAULT_SIZE);
    Allocator a = arena_allocator(arena);

    CervRequest *req = parse_req(reqbuf, buf_s, a);
    if (req == NULL) {
      fprintf(stderr, "malformed request\n");
      free(reqbuf);
      close(clfd);
      continue;
    }

    CervResponse *res = cerv_response_new(a);

    CervHandler *handler = cerv_router_match(s->router, req->method, req->path);
    if (handler != NULL) {
      handler->handle(handler, req, res);
    } else {
      cerv_response_set_status(res, 404);
      cerv_response_set_body(res, "");
    }

    char *outbuf = cerv_response_serialize(res);
    int len = strlen(outbuf);

    send(clfd, outbuf, len, 0);

    free(reqbuf);
    arena_destroy(arena);
    close(clfd);
  }

  freeaddrinfo(result);
  close(socket_d);

  return CERV_DEF_RET_OK;
}
