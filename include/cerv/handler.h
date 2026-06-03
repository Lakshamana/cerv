#ifndef CERV_HANDLER_H
#define CERV_HANDLER_H

#include "request.h"
#include "response.h"

/**
 * CervHandler is a vtable. Client code embeds this as the first field
 * of their concrete handler struct, enabling safe upcasting:
 *
 *   typedef struct {
 *     CervHandler vtable;   // must be first
 *     MyDB *db;
 *   } MyHandler;
 *
 *   void my_handle(void *self, CervRequest *req, CervResponse *res) { ... }
 *
 *   MyHandler h = { .vtable = { my_handle, NULL }, .db = db };
 *   cerv_router_add(&router, "GET", "/hello", &h);
 */
typedef struct {
  void      (*handle)(void *self, CervRequest *req, CervResponse *res);
  void      (*destroy)(void *self); // optional cleanup; NULL = no-op
} CervHandler;

#endif // CERV_HANDLER_H
