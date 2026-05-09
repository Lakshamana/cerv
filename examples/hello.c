#include "cerv/cerv.h"
int main(void) {
  CervServer* server = cerv_new(3000, 10);

  cerv_run(server);
  return 0;
}
