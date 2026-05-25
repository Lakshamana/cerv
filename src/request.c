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

  return CERV_DEF_RET_SUCCESS;
}

int handle_on_body(llhttp_t *p, const char *at, size_t len) {
  CervRequest *req = p->data;

  req->body = realloc(req->body, req->body_len + len + 1);
  memcpy(req->body + req->body_len, at, len);
  req->body_len += len;
  req->body[req->body_len] = '\0';

  return CERV_DEF_RET_SUCCESS;
}

int handle_on_msg_complete(llhttp_t *p) {
  int *done = malloc(sizeof(int));
  *done = 1;

  p->data = done;

  return CERV_DEF_RET_SUCCESS;
}

CervRequest *parse_req(const char *body, size_t len) {
  llhttp_t parser;
  llhttp_settings_t settings;
  CervRequest parsed_r = {0};

  llhttp_settings_init(&settings);
  settings.on_url = handle_on_url;
  settings.on_body = handle_on_body;

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
  free(r->body);
  free(r->path);
  free(r);
}

int parse_qs(RequestParam *params, const char *qs) {
  char *tmp = (char *)qs;
  char *buf = tmp;
  size_t idx = 0;

  while (*tmp != '?') {
    tmp++;
  }
  tmp++; // +1 to skip '?'
  buf = tmp;

  if (tmp >= qs + strlen(qs)) {
    return -1;
  }

  while (*tmp++) {
    // extract key
    if (*tmp == '=') {
      char *key = malloc(tmp - buf + 1);
      key[0] = '\0';
      strncpy(key, buf, (ptrdiff_t)(tmp - buf));

      params[idx].key = key;
      buf = tmp + 1;
      tmp++;
    }

    // extract value
    if (*tmp == '\0' || *tmp == '&') {
      char *val = malloc(tmp - buf + 1);
      val[0] = '\0';
      strncpy(val, buf, (ptrdiff_t)(tmp - buf));

      params[idx].value = val;
      buf = tmp + 1;
      idx++;
      if (*tmp != '\0')
        tmp++;
    }
  }

  return 0;
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

