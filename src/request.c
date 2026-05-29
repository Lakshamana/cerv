#include "cerv/request.h"
#include "cerv/defs.h"
#include "llhttp.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_on_url(llhttp_t *p, const char *at, size_t len) {
  CervRequest *req = p->data;

  req->path = realloc(req->path, req->path_len + len + 1);
  memcpy(req->path + req->path_len, at, len);
  req->path_len += len;
  req->path[req->path_len] = '\0';

  return CERV_DEF_RET_OK;
}

int handle_on_body(llhttp_t *p, const char *at, size_t len) {
  CervRequest *req = p->data;

  req->body = realloc(req->body, req->body_len + len + 1);
  memcpy(req->body + req->body_len, at, len);
  req->body_len += len;
  req->body[req->body_len] = '\0';

  return CERV_DEF_RET_OK;
}

int handle_on_url_complete(llhttp_t *p) {
  CervRequest *req = p->data;

  int qs_result = parse_qs(req->query_string_params, req->path);
  if (qs_result == -1) {
    return CERV_DEF_RET_ERROR;
  }

  if (qs_result > 0) {
    char *qm_at = strchr(req->path, '?');
    size_t len = qm_at - req->path;
    req->path = realloc(req->path, len + 1);
    req->path[len] = '\0';
    req->query_string_count = qs_result;
  }

  return CERV_DEF_RET_OK;
}

int handle_on_msg_complete(llhttp_t *p) {
  int *done = malloc(sizeof(int));
  *done = 1;

  p->data = done;

  return CERV_DEF_RET_OK;
}

int handle_on_header_field(llhttp_t *p, const char *field, size_t len) {
  CervRequest *req = p->data;
  req->headers[req->headers_count].key = strndup(field, len);
  return CERV_DEF_RET_OK;
}

int handle_on_header_value(llhttp_t *p, const char *value, size_t len) {
  CervRequest *req = p->data;
  req->headers[req->headers_count++].value = strndup(value, len);
  return CERV_DEF_RET_OK;
}

CervRequest *parse_req(const char *body, size_t len) {
  llhttp_t parser;
  llhttp_settings_t settings;
  CervRequest parsed_r = {0};

  llhttp_settings_init(&settings);
  settings.on_url = handle_on_url;
  settings.on_url_complete = handle_on_url_complete;
  settings.on_body = handle_on_body;
  settings.on_header_field = handle_on_header_field;
  settings.on_header_value = handle_on_header_value;

  llhttp_init(&parser, HTTP_BOTH, &settings);
  parser.data = &parsed_r;

  enum llhttp_errno err = llhttp_execute(&parser, body, len);
  if (err != HPE_OK) {
    fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err),
            llhttp_get_error_reason(&parser));
    return NULL;
  }

  CervRequest *req = malloc(sizeof(CervRequest));
  memcpy(req, parser.data, sizeof(CervRequest));

  req->method = llhttp_method_name(llhttp_get_method(&parser));

  return req;
}

void check_done(const char *chunk, size_t s, int *done) {
  llhttp_t parser;
  llhttp_settings_t settings;

  llhttp_settings_init(&settings);
  settings.on_message_complete = handle_on_msg_complete;

  llhttp_init(&parser, HTTP_BOTH, &settings);
  parser.data = done;

  enum llhttp_errno err = llhttp_execute(&parser, chunk, s);
  if (err != HPE_OK) {
    fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err),
            llhttp_get_error_reason(&parser));
    *done = -1;
    return;
  }

  *done = *(int *)parser.data;
}

void close_req(CervRequest *r) {
  for (size_t i = 0; i < r->query_string_count; i++) {
    free((char *)r->query_string_params[i].key);
    free((char *)r->query_string_params[i].value);
  }

  for (size_t i = 0 ; i < r->headers_count; i++) {
    free((char *)r->headers[i].key);
    free((char *)r->headers[i].value);
  }

  free(r->body);
  free(r->path);
  free(r);
}

int parse_qs(RequestParam *params, const char *qs) {
  const char *p = strchr(qs, '?');

  // no params
  if (p == NULL)
    return 0;
  p++; // skip '?'
  if (*p == '\0')
    return 0;

  int count = 0;

  while (*p && count < CERV_MAX_REQ_PARAMS) {
    const char *eq = strchr(p, '=');
    const char *amp = strchr(p, '&');

    // malformed URL params
    if (eq == NULL || (amp != NULL && amp < eq))
      return -1;
    if (eq == p)
      return -1;

    params[count].key = strndup(p, eq - p);
    p = eq + 1; // skip past '='

    amp = strchr(p, '&');
    size_t val_len = amp ? (size_t)(amp - p) : strlen(p);
    params[count].value = strndup(p, val_len);
    count++;

    if (amp == NULL)
      break;
    p = amp + 1; // skip past '&'
    if (*p == '\0')
      break;
  }

  return count;
}

const char *qs_get(RequestParam *params, const char *key) {
  for (int i = 0; params[i].key != NULL && i < CERV_MAX_REQ_PARAMS; i++) {
    register RequestParam param = params[i];
    if (strcmp(key, param.key) == 0) {
      return param.value;
    }
  }

  return NULL;
}
