#ifndef CERV_RESPONSE_H
#define CERV_RESPONSE_H

typedef struct {
  int   status;       // HTTP status code
  char *body;         // response body (heap-allocated by framework)
  char *content_type; // e.g. "application/json"
} CervResponse;

CervResponse* cerv_response_new();
void          cerv_response_set_body(CervResponse *res, const char *body);
void          cerv_response_set_status(CervResponse *res, int status);
void          close_res(CervResponse* res);
char*         cerv_response_serialize(CervResponse* res);

#endif // CERV_RESPONSE_H
