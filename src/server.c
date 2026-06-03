#include "cerv/arena.h"
#include "cerv/cerv.h"
#include "cerv/defs.h"
#include "cerv/handler.h"
#include "cerv/request.h"
#include "cerv/response.h"
#include "cerv/router.h"
#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

Cerv *cerv_new(int port) {
  Cerv *s = calloc(1, sizeof(Cerv));
  CervRouter *r = cerv_router_new();
  s->workers = CERV_DEF_MIN_WORKERS;
  s->port = port;
  s->router = r;

  return s;
}

int cerv_run(Cerv *s) {
  int status, socket_d;
  struct addrinfo hint;
  struct addrinfo *result;

  // NOTE: APUE, Ch. 16, pg. 600: remaining integer fields must be set to 0
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
    fprintf(stderr, "error allocating socket: %d", errno);
    freeaddrinfo(result);
    return CERV_DEF_RET_ERROR;
  }

  int opt = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if ((bind(socket_d, result->ai_addr, result->ai_addrlen)) != 0) {
    fprintf(stderr, "error binding to socket err: %d\n", errno);
    freeaddrinfo(result);
    close(socket_d);
    return CERV_DEF_RET_ERROR;
  }

  if (listen(socket_d, CERV_DEF_MAX_BACKLOG) == -1) {
    fprintf(stderr, "error listening to the port=%d: %d", s->port, errno);
    return CERV_DEF_RET_ERROR;
  }

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
