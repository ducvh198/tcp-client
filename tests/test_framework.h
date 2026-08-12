#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s... ", #test_func); \
        g_tests_run++; \
        int failed_before = g_tests_failed; \
        test_func(); \
        if (g_tests_failed == failed_before) { \
            g_tests_passed++; \
            printf("[PASS]\n"); \
        } else { \
            printf("[FAIL]\n"); \
        } \
    } while (0)

#define ASSERT_EQ_INT(expected, actual) \
    do { \
        int exp_val = (int)(expected); \
        int act_val = (int)(actual); \
        if (exp_val != act_val) { \
            printf("\n  ASSERTION FAILED at %s:%d: Expected %d, got %d\n", \
                   __FILE__, __LINE__, exp_val, act_val); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_STREQ(expected, actual) \
    do { \
        const char *exp_str = (expected); \
        const char *act_str = (actual); \
        if (!exp_str || !act_str || strcmp(exp_str, act_str) != 0) { \
            printf("\n  ASSERTION FAILED at %s:%d: Expected \"%s\", got \"%s\"\n", \
                   __FILE__, __LINE__, exp_str ? exp_str : "NULL", act_str ? act_str : "NULL"); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("\n  ASSERTION FAILED at %s:%d: Condition (" #condition ") is false\n", \
                   __FILE__, __LINE__); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
