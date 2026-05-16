#ifndef CERV_REQUEST_H
#define CERV_REQUEST_H

#include <stddef.h>

typedef struct {
  const char *method; // "GET", "POST", ...
  char       *path;   // "/hello"
  size_t     path_len;
  char       *body; // raw request body (may be NULL)
  size_t     body_len;
} CervRequest;

CervRequest* parse_req(const char* body, size_t len);
void         close_req(CervRequest* r);

#endif // CERV_REQUEST_H
