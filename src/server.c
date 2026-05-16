#include "cerv/cerv.h"
#include "cerv/defs.h"
#include "cerv/handler.h"
#include "cerv/request.h"
#include "cerv/response.h"
#include "cerv/router.h"
#include <errno.h>
#include <netdb.h>
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
    char *reqbuf = calloc(1, 4096);
    if ((n_bytes = recv(clfd, reqbuf, 4096, 0)) < 0) {
      fprintf(stderr, "error receiving msg from client: %d\n", errno);
      continue;
    }

    CervRequest *req = parse_req(reqbuf, n_bytes);
    CervHandler *handler = cerv_router_match(s->router, req->method, req->path);
    if (handler == NULL) {
      fprintf(stderr, "route not found!");
    }

    CervResponse *res = cerv_response_new();
    handler->handle(handler, req, res);

    char *resp_body = cerv_response_serialize(res);
    int len, bytes_sent;
    len = strlen(resp_body);

    bytes_sent = send(clfd, resp_body, len, 0);

    free(reqbuf);
    close_req(req);
    close_res(res);
    close(clfd);
    printf("%s\n", resp_body);
    printf("Bytes sent: %d\n", bytes_sent);
  }

  freeaddrinfo(res);
  close(socket_d);

  return CERV_DEF_RET_SUCCESS;
}
