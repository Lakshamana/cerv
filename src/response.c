#include "cerv/response.h"
#include <stdlib.h>
#include <string.h>

void cerv_response_set_body(CervResponse *res, const char *body) {
  char *b = malloc(strlen(body) + 1);
  b[0] = '\0';
  strcpy(b, body);
  res->body = b;
}

void cerv_response_set_status(CervResponse *res, int status) {
  res->status = status;
}
