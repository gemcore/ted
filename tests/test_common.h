/*
 * test_common.h - Minimal unit test helpers for host-side tests.
 */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <string.h>

static int test_checks = 0;
static int test_failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        test_checks++;                                                  \
        if (!(cond)) {                                                  \
            test_failures++;                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                               \
    } while (0)

#define CHECK_STR(actual, expected)                                     \
    do {                                                                \
        test_checks++;                                                  \
        if (strcmp((actual), (expected)) != 0) {                        \
            test_failures++;                                            \
            printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__,          \
                   __LINE__, (actual), (expected));                     \
        }                                                               \
    } while (0)

#define TEST_SUMMARY()                                                  \
    do {                                                                \
        printf("%s: %d checks, %d failures\n", __FILE__,                \
               test_checks, test_failures);                             \
        return test_failures == 0 ? 0 : 1;                              \
    } while (0)

#endif /* TEST_COMMON_H */
