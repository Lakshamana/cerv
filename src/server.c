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

CervServer *cerv_new(int port, int max_workers) {
  CervServer *s = malloc(sizeof(CervServer));
  CervRouter *r = cerv_router_new();
  s->max_workers = max_workers;
  s->port = port;
  s->router = r;

  return s;
}

int cerv_run(CervServer *s) {
  int status, socket_d;
  struct addrinfo hint;
  struct addrinfo *res;

  // NOTE: APUE, Ch. 16, pg. 600: remaining integer fields must be set to 0
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", s->port);
  if ((status = getaddrinfo(NULL, port_str, &hint, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return CERV_DEF_RET_ERROR;
  }

  socket_d = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (socket_d == -1) {
    fprintf(stderr, "error allocating socket: %d", errno);
    freeaddrinfo(res);
    return CERV_DEF_RET_ERROR;
  }

  int opt = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if ((bind(socket_d, res->ai_addr, res->ai_addrlen)) != 0) {
    fprintf(stderr, "error binding to socket err: %d\n", errno);
    freeaddrinfo(res);
    close(socket_d);
    return CERV_DEF_RET_ERROR;
  }
  freeaddrinfo(res);

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

    CervRequest *req = parse_req(reqbuf, buf_s);
    if (req == NULL) {
      fprintf(stderr, "malformed request\n");
      free(reqbuf);
      close(clfd);
      continue;
    }

    CervResponse *res = cerv_response_new();

    CervHandler *handler = cerv_router_match(s->router, req->method, req->path);
    if (handler != NULL) {
      handler->handle(handler, req, res);
    } else {
      cerv_response_set_status(res, 404);
      cerv_response_set_body(res, "");
    }

    char *resp_body = cerv_response_serialize(res);
    int len;
    len = strlen(resp_body);

    send(clfd, resp_body, len, 0);

    free(reqbuf);
    close_req(req);
    close_res(res);
    close(clfd);
  }

  freeaddrinfo(res);
  close(socket_d);

  return CERV_DEF_RET_OK;
}
