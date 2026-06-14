#ifndef CERV_REQUEST_H
#define CERV_REQUEST_H

#include "cerv/arena.h"
#include <stdlib.h>
#include <string.h>
#define CERV_MAX_REQ_HEADERS 32
#define CERV_MAX_REQ_PARAMS 16

#include <stddef.h>

typedef struct {
  const char *key;
  const char *value;
} RequestHeader;

typedef struct {
  const char *key;
  const char *value;
} RequestParam;

typedef struct {
  const char*   method;
  char*         path;
  size_t        path_len;
  char*         body;
  size_t        body_len;
  RequestHeader headers[CERV_MAX_REQ_HEADERS];
  size_t        headers_count;
  RequestParam  query_string_params[CERV_MAX_REQ_PARAMS];
  size_t        query_string_count;
} CervRequest;

CervRequest* parse_req(const char *body, size_t len, Allocator a);
void         close_req(Arena* a);
void         check_done(const char *chunk, size_t s, int *done);
int          parse_qs(RequestParam *params, const char *qs);
const char*  qs_get(RequestParam *params, const char *key);

#endif // CERV_REQUEST_H
