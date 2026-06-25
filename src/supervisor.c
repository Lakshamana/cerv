#include "cerv/supervisor.h"
#include "cerv/defs.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

static CervSharedRegion *supervisor_shm_alloc(int n_workers) {
  size_t n_slots = n_workers * CERV_DEF_MAX_FIBERS_PER_WORKER;
  size_t size = sizeof(CervSharedRegion) +
                n_workers * sizeof(CervWorkerStatus) + n_slots +
                n_slots * CERV_DEF_ARENA_SLOT_SIZE +
                n_slots * sizeof(CervFiberCheckpoint);

  void *base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  CervSharedRegion *r = (CervSharedRegion *)base;
  uint8_t *cursor = (uint8_t *)base + sizeof(CervSharedRegion);

  // set base and size headers
  r->base = base;
  r->n_slots = n_slots;
  r->total_size = size;

  // set worker status list
  r->worker_status = (CervWorkerStatus *)cursor;
  cursor += n_workers * sizeof(CervWorkerStatus);

  // set free slots bitset
  r->free_list = cursor;
  cursor += n_slots;

  // set checkpoints 
  r->checkpoints = (CervFiberCheckpoint*)cursor;
  cursor += n_slots * sizeof(CervFiberCheckpoint);

  r->arena_slots = cursor;

  return r;
}

static void supervisor_shm_free(CervSharedRegion *region) {
  munmap(region, region->total_size);
}
