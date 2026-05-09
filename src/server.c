#include "cerv/cerv.h"
#include "cerv/defs.h"
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
    return CERV_DEF_RET_ERROR;
  }

  if ((bind(socket_d, res->ai_addr, res->ai_addrlen)) != 0) {
    fprintf(stderr, "error binding to socket err: %d\n", errno);
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

    int recv_status;
    char* req_buf = calloc(1, 4096);
    if ((recv_status = recv(clfd, req_buf, 4096, 0)) < 0) {
      fprintf(stderr, "error receiving msg from client: %d\n", errno);
      continue;
    }

    printf("Message received: %s\n", req_buf);
    printf("wait for the response...\n");

    char *resp = "world!";
    int len, bytes_sent;
    len = strlen(resp);

    bytes_sent = send(clfd, resp, len, 0);

    free(req_buf);
    close(clfd);
    printf("%s\n", resp);
    printf("Bytes sent: %d\n", bytes_sent);
  }

  freeaddrinfo(res);
  close(socket_d);

  return CERV_DEF_RET_SUCCESS;
}
