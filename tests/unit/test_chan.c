#include "cerv/chan.h"
#include "test.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static int passed = 0, failed = 0;

/* ── chan_open / is_chan_ok ──────────────────────────────────────────────── */

static void test_chan_open() {
  TEST(chan_open);

  Chan c = chan_open();

  ASSERT(is_chan_ok(c), "channel should be ok after open");
  ASSERT(c.r_fd >= 0, "read fd should be non-negative");
  ASSERT(c.w_fd >= 0, "write fd should be non-negative");
  ASSERT(c.r_fd != c.w_fd, "read and write fds should differ");

  chan_close(c);
}

static void test_chan_open_fds_are_valid() {
  TEST(chan_open_fds_are_valid);

  Chan c = chan_open();

  // write should succeed on a valid write fd
  int res = write(c.w_fd, "x", 1);
  ASSERT(res == 1, "write to w_fd should succeed");

  // read should return what was written
  char buf;
  res = read(c.r_fd, &buf, 1);
  ASSERT(res == 1, "read from r_fd should succeed");
  ASSERT(buf == 'x', "read byte should match written byte");

  chan_close(c);
}

/* ── chan_send / chan_recv ───────────────────────────────────────────────── */

static void test_send_recv_single_byte() {
  TEST(send_recv_single_byte);

  Chan c = chan_open();
  char sent = 'A';
  char recvd = 0;

  int s = chan_send(c, &sent, sizeof(sent));
  ASSERT(s == 1, "chan_send should return 1 byte written");

  int r = chan_recv(c, &recvd, sizeof(recvd));
  ASSERT(r == 1, "chan_recv should return 1 byte read");
  ASSERT(recvd == 'A', "received byte should match sent byte");

  chan_close(c);
}

static void test_send_recv_struct() {
  TEST(send_recv_struct);

  typedef struct {
    int job_id;
    int slot_index;
    uint32_t payload_len;
  } JobPacket;

  Chan c = chan_open();
  JobPacket sent = {.job_id = 42, .slot_index = 7, .payload_len = 1024};
  JobPacket recvd = {0};

  int s = chan_send(c, &sent, sizeof(sent));
  ASSERT(s == (int)sizeof(JobPacket), "chan_send should write full struct");

  int r = chan_recv(c, &recvd, sizeof(recvd));
  ASSERT(r == (int)sizeof(JobPacket), "chan_recv should read full struct");
  ASSERT_INT_EQ(recvd.job_id, 42, "job_id should match");
  ASSERT_INT_EQ(recvd.slot_index, 7, "slot_index should match");
  ASSERT_INT_EQ((int)recvd.payload_len, 1024, "payload_len should match");

  chan_close(c);
}

static void test_send_recv_multiple_messages() {
  TEST(send_recv_multiple_messages);

  Chan c = chan_open();
  int vals[] = {10, 20, 30};

  for (int i = 0; i < 3; i++)
    chan_send(c, &vals[i], sizeof(int));

  for (int i = 0; i < 3; i++) {
    int got;
    chan_recv(c, &got, sizeof(int));
    ASSERT_INT_EQ(got, vals[i], "message order should be preserved");
  }

  chan_close(c);
}

/* ── chan_try_recv ───────────────────────────────────────────────────────── */

static void test_try_recv_empty() {
  TEST(try_recv_empty);

  Chan c = chan_open();
  char buf;

  int r = chan_try_recv(c, &buf, sizeof(buf));
  ASSERT_INT_EQ(r, -1, "try_recv on empty pipe should return -1");
  ASSERT_INT_EQ(errno, EAGAIN, "errno should be EAGAIN when no data");

  chan_close(c);
}

static void test_try_recv_with_data() {
  TEST(try_recv_with_data);

  Chan c = chan_open();
  int sent = 99;
  int recvd = 0;

  chan_send(c, &sent, sizeof(sent));

  int r = chan_try_recv(c, &recvd, sizeof(recvd));
  ASSERT(r == (int)sizeof(int), "try_recv should return bytes read when data available");
  ASSERT_INT_EQ(recvd, 99, "try_recv should read the sent value");

  chan_close(c);
}

/* ── chan_close ──────────────────────────────────────────────────────────── */

static void test_close_recv_eof() {
  TEST(close_recv_eof);

  Chan c = chan_open();

  // close write end — reader should get EOF
  close(c.w_fd);

  char buf;
  int r = chan_recv(c, &buf, sizeof(buf));
  ASSERT_INT_EQ(r, 0, "recv after write end closed should return 0 (EOF)");

  close(c.r_fd);
}

static void test_close_send_epipe() {
  TEST(close_send_epipe);

  // suppress SIGPIPE so write returns -1 instead of killing the process
  signal(SIGPIPE, SIG_IGN);

  Chan c = chan_open();

  // close read end — writer should get error
  close(c.r_fd);

  int r = chan_send(c, "x", 1);
  ASSERT_INT_EQ(r, -1, "send after read end closed should return -1");

  close(c.w_fd);
  signal(SIGPIPE, SIG_DFL);
}

/* ── is_chan_ok ──────────────────────────────────────────────────────────── */

static void test_is_chan_ok_valid() {
  TEST(is_chan_ok_valid);

  Chan c = chan_open();
  ASSERT_INT_EQ(is_chan_ok(c), 1, "is_chan_ok should return 1 for valid channel");
  chan_close(c);
}

static void test_is_chan_ok_invalid() {
  TEST(is_chan_ok_invalid);

  Chan bad = {-1, -1};
  ASSERT_INT_EQ(is_chan_ok(bad), 0, "is_chan_ok should return 0 for invalid channel");
}

/* ── cross-process ──────────────────────────────────────────────────────── */

static void test_cross_process_send_recv() {
  TEST(cross_process_send_recv);

  Chan c = chan_open();
  pid_t pid = fork();

  if (pid == 0) {
    // child: close read end, send a value, exit
    close(c.r_fd);
    int val = 777;
    chan_send(c, &val, sizeof(val));
    close(c.w_fd);
    _exit(0);
  }

  // parent: close write end, recv the value
  close(c.w_fd);
  int got = 0;
  int r = chan_recv(c, &got, sizeof(got));
  ASSERT(r == (int)sizeof(int), "parent should read full int from child");
  ASSERT_INT_EQ(got, 777, "parent should receive value sent by child");

  close(c.r_fd);

  // reap child
  int status;
  waitpid(pid, &status, 0);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
  typedef void (*TestFn)(void);
  struct { const char *name; TestFn fn; } tests[] = {
    {"test_chan_open",                test_chan_open},
    {"test_chan_open_fds_are_valid",  test_chan_open_fds_are_valid},
    {"test_send_recv_single_byte",   test_send_recv_single_byte},
    {"test_send_recv_struct",        test_send_recv_struct},
    {"test_send_recv_multiple_messages", test_send_recv_multiple_messages},
    {"test_try_recv_empty",          test_try_recv_empty},
    {"test_try_recv_with_data",      test_try_recv_with_data},
    {"test_close_recv_eof",          test_close_recv_eof},
    {"test_close_send_epipe",        test_close_send_epipe},
    {"test_is_chan_ok_valid",         test_is_chan_ok_valid},
    {"test_is_chan_ok_invalid",       test_is_chan_ok_invalid},
    {"test_cross_process_send_recv", test_cross_process_send_recv},
  };
  size_t ntests = sizeof(tests) / sizeof(tests[0]);

  if (argc == 2) {
    for (size_t i = 0; i < ntests; i++) {
      if (strcmp(argv[1], tests[i].name) == 0) {
        tests[i].fn();
        printf("%s\n%d assertions passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR, passed, failed);
        return failed > 0 ? 1 : 0;
      }
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
  }

  for (size_t i = 0; i < ntests; i++)
    tests[i].fn();

  printf("%s\n%d assertions passed, %d failed\n\n", failed > 0 ? _FAIL_STR : _PASS_STR,
         passed, failed);

  return failed > 0 ? 1 : 0;
}
