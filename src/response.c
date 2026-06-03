#include "cerv/response.h"
#include "cerv/arena.h"
#include "cerv/defs.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CervResponse *cerv_response_new(Allocator a) {
  CervResponse *resp = cerv_make(CervResponse, a);
  resp->allocator = a;
  return resp;
}

void close_res(CervResponse *r) {
  for (size_t i = 0; i < r->headers_count; i++) {
    free((char *)r->headers[i].key);
    free((char *)r->headers[i].value);
  }

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
  char *b = cerv_alloc(char, strlen(body) + 1, res->allocator);
  b[0] = '\0';
  strcpy(b, body);
  res->body = b;
}

void cerv_response_set_status(CervResponse *res, int status) {
  res->status = status;
}

void cerv_response_set_header(CervResponse *res, const char *key,
                              const char *value) {
  char *k = cerv_alloc(char, strlen(key) + 1, res->allocator);
  k[0] = '\0';
  strcpy(k, key);
  res->headers[res->headers_count].key = k;

  char *v = cerv_alloc(char, strlen(value) + 1, res->allocator);
  v[0] = '\0';
  strcpy(v, value);
  res->headers[res->headers_count].value = v;
  res->headers_count++;
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
  const char *ct   = res->content_type ? res->content_type : "text/plain";
  size_t body_len  = strlen(body);

  int fixed_len = snprintf(NULL, 0,
      "%s %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %zu\r\n",
      CERV_DEF_DEFAULT_PROTOCOL, res->status, reason, ct, body_len);

  size_t extra_len = 0;
  for (size_t i = 0; i < res->headers_count; i++)
    extra_len += strlen(res->headers[i].key) + 2 + strlen(res->headers[i].value) + 2;

  char *out = cerv_alloc(char, (size_t)fixed_len + extra_len + 2 + body_len + 1, res->allocator);

  size_t pos = (size_t)snprintf(out, (size_t)fixed_len + 1,
      "%s %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %zu\r\n",
      CERV_DEF_DEFAULT_PROTOCOL, res->status, reason, ct, body_len);

  for (size_t i = 0; i < res->headers_count; i++)
    pos += (size_t)sprintf(out + pos, "%s: %s\r\n", res->headers[i].key, res->headers[i].value);

  out[pos++] = '\r';
  out[pos++] = '\n';
  memcpy(out + pos, body, body_len);
  out[pos + body_len] = '\0';

  return out;
}
