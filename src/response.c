#include "cerv/response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CervResponse *cerv_response_new() { return calloc(1, sizeof(CervResponse)); }

void close_res(CervResponse *r) {
  if (r->content_type != NULL) {
    free(r->content_type);
  }

  if (r->body != NULL) {
    free(r->body);
  }

  if (r != NULL) {
    free(r);
  }
}

void cerv_response_set_body(CervResponse *res, const char *body) {
  char *b = malloc(strlen(body) + 1);
  b[0] = '\0';
  strcpy(b, body);
  res->body = b;
}

void cerv_response_set_status(CervResponse *res, int status) {
  res->status = status;
}

void cerv_response_set_content_type(CervResponse *res, const char* ctype) {
  char *t = malloc(strlen(ctype) + 1);
  t[0] = '\0';
  strcpy(t, ctype);
  res->content_type = t;
}

char *cerv_response_serialize(CervResponse *res) {
  const char *reason;
  switch (res->status) {
  case 200:
    reason = "OK";
    break;
  case 201:
    reason = "Created";
    break;
  case 204:
    reason = "No Content";
    break;
  case 400:
    reason = "Bad Request";
    break;
  case 404:
    reason = "Not Found";
    break;
  case 500:
    reason = "Internal Server Error";
    break;
  default:
    reason = "Unknown";
    break;
  }

  const char *body = res->body ? res->body : "";
  const char *ct = res->content_type ? res->content_type : "text/plain";
  size_t body_len = strlen(body);

  int hdr_len = snprintf(NULL, 0,
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         res->status, reason, ct, body_len);

  char *out = malloc(hdr_len + body_len + 1);
  snprintf(out, hdr_len + 1,
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %zu\r\n"
           "\r\n",
           res->status, reason, ct, body_len);

  memcpy(out + hdr_len, body, body_len);
  out[hdr_len + body_len] = '\0';

  return out;
}
