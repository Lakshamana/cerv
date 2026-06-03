#ifndef CERV_RESPONSE_H
#define CERV_RESPONSE_H

#include "cerv/arena.h"
#define CERV_MAX_RESPONSE_HEADERS 32

#include <stddef.h>

typedef struct {
  const char *key;
  const char *value;
} ResponseHeader;

typedef struct {
  int             status;
  size_t          headers_count;
  ResponseHeader  headers[CERV_MAX_RESPONSE_HEADERS];
  char*           body;
  char*           content_type;
  char*           protocol;
  Allocator       allocator;
} CervResponse;

CervResponse* cerv_response_new(Allocator a);
void          cerv_response_set_body(CervResponse *res, const char *body);
void          cerv_response_set_status(CervResponse *res, int status);
void          cerv_response_set_header(CervResponse *res, const char* key, const char* value);
char*         cerv_response_serialize(CervResponse* res);

#endif // CERV_RESPONSE_H
