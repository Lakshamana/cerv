#ifndef CERV_REQUEST_H
#define CERV_REQUEST_H

#include <stddef.h>

typedef struct {
  const char *method;   // "GET", "POST", ...
  const char *path;     // "/hello"
  const char *body;     // raw request body (may be NULL)
  size_t      body_len;
} CervRequest;

#endif // CERV_REQUEST_H
