#include "cerv/process.h"
#include "test.h"
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

static int passed = 0, failed = 0;

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void noop_spawn(void *ctx) { (void)ctx; }

static void spawn_write_42(void *ctx) {
  int w_fd = *(int *)ctx;
  uint8_t val = 42;
  write(w_fd, &val, 1);
}

static void supervised_noop(void *ctx, Chan *ready, int stop_fd) {
  (void)ctx;
  uint8_t rdy = 1;
  chan_send(*ready, &rdy, 1);
  uint8_t buf;
  read(stop_fd, &buf, 1); // blocks until parent closes stop_w_fd (EOF)
}

typedef struct {
  int before_ready_w_fd; // child writes here before signalling ready
  int after_stop_w_fd;   // child writes here after detecting stop EOF
} LifecycleCtx;

static void supervised_lifecycle(void *ctx, Chan *ready, int stop_fd) {
  LifecycleCtx *c = ctx;
  uint8_t pre = 1;
  write(c->before_ready_w_fd, &pre, 1);
  uint8_t rdy = 1;
  chan_send(*ready, &rdy, 1);
  uint8_t buf;
  read(stop_fd, &buf, 1); // blocks until EOF
  uint8_t post = 2;
  write(c->after_stop_w_fd, &post, 1);
}

/* ── spawn ───────────────────────────────────────────────────────────────── */

static void test_spawn_returns_valid_pid() {
  TEST(spawn_returns_valid_pid);

  Process p = spawn(NULL, noop_spawn);

  ASSERT(p.pid > 0, "spawn should return a positive pid");
  ASSERT_INT_EQ(p.stop_w_fd, -1, "spawn should have no stop pipe");

  waitpid(p.pid, NULL, 0);
}

static void test_spawn_child_executes_callback() {
  TEST(spawn_child_executes_callback);

  Chan verify = chan_open();
  int w_fd    = verify.w_fd;

  Process p = spawn(&w_fd, spawn_write_42);
  close(verify.w_fd);

  uint8_t got = 0;
  int r = chan_recv(verify, &got, 1);
  ASSERT(r == 1, "should receive byte written by child callback");
  ASSERT_INT_EQ(got, 42, "child callback should write 42");

  close(verify.r_fd);
  waitpid(p.pid, NULL, 0);
}

/* ── supervise ───────────────────────────────────────────────────────────── */

static void test_supervise_returns_valid_process() {
  TEST(supervise_returns_valid_process);

  Process p = supervise(NULL, supervised_noop);

  ASSERT(p.pid > 0, "supervise should return a positive pid");
  ASSERT(p.stop_w_fd >= 0, "supervise should return a valid stop write fd");

  close(p.stop_w_fd);
  waitpid(p.pid, NULL, 0);
}

static void test_supervise_stop_signals_child() {
  TEST(supervise_stop_signals_child);

  // child blocks on stop_fd after signalling ready; closing stop_w_fd
  // delivers EOF, child exits -- waitpid confirms it terminated cleanly
  Process p = supervise(NULL, supervised_noop);

  ASSERT(p.pid > 0, "process should be valid before stop");

  close(p.stop_w_fd);

  int status = 0;
  waitpid(p.pid, &status, 0);
  ASSERT(WIFEXITED(status), "child should exit normally after stop");
  ASSERT_INT_EQ(WEXITSTATUS(status), 0, "child exit code should be 0");
}

static void test_supervise_full_lifecycle() {
  TEST(supervise_full_lifecycle);

  Chan before = chan_open();
  Chan after  = chan_open();

  LifecycleCtx ctx = {
    .before_ready_w_fd = before.w_fd,
    .after_stop_w_fd   = after.w_fd,
  };

  // supervise blocks until child sends ready -- by the time it returns,
  // the child has already written to before_ready_w_fd
  Process p = supervise(&ctx, supervised_lifecycle);
  close(before.w_fd);
  close(after.w_fd);

  uint8_t pre = 0;
  int r = chan_recv(before, &pre, 1);
  ASSERT(r == 1, "should receive pre-ready byte");
  ASSERT_INT_EQ(pre, 1, "pre-ready value should be 1 (written before ready signal)");

  close(p.stop_w_fd); // EOF on child's stop_fd

  uint8_t post = 0;
  r = chan_recv(after, &post, 1);
  ASSERT(r == 1, "should receive post-stop byte");
  ASSERT_INT_EQ(post, 2, "post-stop value should be 2 (written after stop detected)");

  close(before.r_fd);
  close(after.r_fd);
  waitpid(p.pid, NULL, 0);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
  typedef void (*TestFn)(void);
  struct { const char *name; TestFn fn; } tests[] = {
    {"test_spawn_returns_valid_pid",         test_spawn_returns_valid_pid},
    {"test_spawn_child_executes_callback",   test_spawn_child_executes_callback},
    {"test_supervise_returns_valid_process", test_supervise_returns_valid_process},
    {"test_supervise_stop_signals_child",    test_supervise_stop_signals_child},
    {"test_supervise_full_lifecycle",        test_supervise_full_lifecycle},
  };
  size_t ntests = sizeof(tests) / sizeof(tests[0]);

  if (argc == 2) {
    for (size_t i = 0; i < ntests; i++) {
      if (__builtin_strcmp(argv[1], tests[i].name) == 0) {
        tests[i].fn();
        printf("%s\n%d assertions passed, %d failed\n\n",
               failed > 0 ? _FAIL_STR : _PASS_STR, passed, failed);
        return failed > 0 ? 1 : 0;
      }
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
  }

  for (size_t i = 0; i < ntests; i++)
    tests[i].fn();

  printf("%s\n%d assertions passed, %d failed\n\n",
         failed > 0 ? _FAIL_STR : _PASS_STR, passed, failed);

  return failed > 0 ? 1 : 0;
}
