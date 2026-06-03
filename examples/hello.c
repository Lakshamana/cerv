#include "cerv/cerv.h"
#include "cerv/request.h"
#include "cerv/response.h"
#include "cerv/router.h"

void handler(void *self, CervRequest *req, CervResponse *res) {
  cerv_response_set_status(res, 200);
  cerv_response_set_body(res, "world!");
}

typedef struct {
  CervHandler vtable; // must be first
} HelloHandler;

int main(void) {
  Cerv *server = cerv_new(3000);

  HelloHandler h = {.vtable = {handler, NULL}};
  cerv_router_add(server->router, "POST", "/", &h);

  cerv_run(server);
  return 0;
}
