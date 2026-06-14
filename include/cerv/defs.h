#ifndef DEFS_H
#define DEFS_H

// function return codes
#define CERV_DEF_RET_OK                0
#define CERV_DEF_RET_ERROR            -1

// server common defs
#define CERV_DEF_MAX_BACKLOG           1024
#define CERV_DEF_READ_CHUNK            4096
#define CERV_DEF_ARENA_DEFAULT_SIZE    1048576
#define CERV_DEF_DEFAULT_PROTOCOL      "HTTP/1.1"
#define CERV_DEF_MIN_WORKERS           8
#define CERV_DEF_MAX_WORKERS           32

// arenas, jobs, fiber defs
#define CERV_DEF_ARENA_SLOT_SIZE           262144  // default: 256KB per job
#define CERV_DEF_FIBER_STACK_SIZE          65536   // default: 64KB per fiber
#define CERV_DEF_MAX_FIBERS_PER_WORKER     64      // default: 64
#define CERV_DEF_JOB_QUARANTINE_THRESHOLD  3       // default: 3 retries before 503
#define CERV_DEF_HEARTBEAT_TIMEOUT_MS      5000    // default: 5000ms before marking worker dead
#define CERV_DEF_DRAIN_TIMEOUT_MS          30000   // default: 30000ms graceful shutdown wait
#define CERV_DEF_MAX_PENDING_JOBS          (CERV_DEF_MAX_WORKERS * CERV_MAX_FIBERS_PER_WORKER)

#endif // !_DEFS_H
