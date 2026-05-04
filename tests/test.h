#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

#define _PASS_STR "\033[32mPASS\033[0m"
#define _FAIL_STR "\033[31mFAIL\033[0m"

static inline void _print_pass(const char *msg) {
  if (msg)
    printf("  " _PASS_STR ": %s\n", msg);
  else
    printf("  " _PASS_STR "\n");
}

static inline void _print_fail(const char *msg) {
  if (msg)
    printf("  " _FAIL_STR ": %s\n", msg);
  else
    printf("  " _FAIL_STR "\n");
}

#define PASS(...) _print_pass(("" __VA_ARGS__)[0] ? ("" __VA_ARGS__) : NULL)
#define FAIL(...) _print_fail(("" __VA_ARGS__)[0] ? ("" __VA_ARGS__) : NULL)

#define ASSERT_STR_EQ(got, expected, ...)                                      \
  do {                                                                         \
    const char *_g = (got);                                                    \
    const char *_e = (expected);                                               \
    if (_g && strcmp(_g, _e) == 0) {                                           \
      PASS(__VA_ARGS__);                                                       \
      passed++;                                                                \
    } else {                                                                   \
      FAIL(__VA_ARGS__);                                                       \
      printf("    got \"%s\", expected \"%s\"\n", _g ? _g : "(null)", _e);    \
      failed++;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_INT_EQ(got, expected, ...)                                      \
  do {                                                                         \
    int _g = (got);                                                            \
    int _e = (expected);                                                       \
    if (_g == _e) {                                                            \
      PASS(__VA_ARGS__);                                                       \
      passed++;                                                                \
    } else {                                                                   \
      FAIL(__VA_ARGS__);                                                       \
      printf("    got %d, expected %d\n", _g, _e);                            \
      failed++;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_NULL(got, ...) ASSERT_INT_EQ((got) == NULL, 1, ##__VA_ARGS__)

#define TEST(name) printf("TEST %s\n", #name)

#endif // TEST_H
