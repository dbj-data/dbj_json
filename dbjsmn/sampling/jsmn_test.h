#ifndef __JSMN_TEST_H__
#define __JSMN_TEST_H__

#include <stdio.h>

static int test_passed = 0;
static int test_failed = 0;

/* Terminate current jsmn_test_run with error */
#define JSMN_FAIL() return __LINE__

/* Successful end of the jsmn_test_run case */
#define JSMN_DONE() return 0

/* Check single condition */
#define JSMN_CHECK(cond)                                                       \
  do {                                                                         \
    if (!(cond))                                                               \
      JSMN_FAIL();                                                             \
  } while (0)

/* Test runner */
static void jsmn_test_run(int (*func)(void), const char *name) {
  int r = func();
  if (r == 0) {
    test_passed++;
    printf("PASSED: %s \n", name);
  } else {
    test_failed++;
    printf("FAILED: %s (at line %d)\n", name, r);
  }
}

#endif /* __JSMN_TEST_H__ */
