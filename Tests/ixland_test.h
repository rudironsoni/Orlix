/*
 * ixland_test.h - Test Framework for libixland
 *
 * Simple, lightweight test framework for syscall testing
 */

#ifndef IXLAND_TEST_H
#define IXLAND_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test statistics */
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} test_stats_t;

static test_stats_t g_test_stats = {0, 0, 0, 0};

/* Color codes for output */
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RED    "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET  "\033[0m"

/* Test assertion macros */
#define TEST_ASSERT(condition) \
    do { \
        g_test_stats.total++; \
        if (condition) { \
            g_test_stats.passed++; \
            printf("  " COLOR_GREEN "✓" COLOR_RESET " %s:%d\n", __FILE__, __LINE__); \
        } else { \
            g_test_stats.failed++; \
            printf("  " COLOR_RED "✗" COLOR_RESET " %s:%d - Assertion failed: %s\n", \
                   __FILE__, __LINE__, #condition); \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))
#define TEST_ASSERT_NE(a, b) TEST_ASSERT((a) != (b))
#define TEST_ASSERT_GT(a, b) TEST_ASSERT((a) > (b))
#define TEST_ASSERT_LT(a, b) TEST_ASSERT((a) < (b))
#define TEST_ASSERT_GE(a, b) TEST_ASSERT((a) >= (b))
#define TEST_ASSERT_LE(a, b) TEST_ASSERT((a) <= (b))

#define TEST_ASSERT_NULL(ptr) TEST_ASSERT((ptr) == NULL)
#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT((ptr) != NULL)
#define TEST_ASSERT_TRUE(cond) TEST_ASSERT(cond)
#define TEST_ASSERT_FALSE(cond) TEST_ASSERT(!(cond))

#define TEST_ASSERT_STR_EQ(a, b) \
    do { \
        g_test_stats.total++; \
        if (strcmp((a), (b)) == 0) { \
            g_test_stats.passed++; \
            printf("  " COLOR_GREEN "✓" COLOR_RESET " String equality at %s:%d\n", __FILE__, __LINE__); \
        } else { \
            g_test_stats.failed++; \
            printf("  " COLOR_RED "✗" COLOR_RESET " %s:%d - Expected \"%s\", got \"%s\"\n", \
                   __FILE__, __LINE__, (b), (a)); \
        } \
    } while(0)

/* Test function declaration macro */
#define TEST(name) void test_##name(void)
#define TEST_EXTERN(name) void test_##name(void)

/* Test suite declaration */
#define TEST_SUITE_BEGIN(suite_name) \
    void run_##suite_name##_tests(void) { \
        printf("\n========================================\n"); \
        printf("Test Suite: %s\n", #suite_name); \
        printf("========================================\n");

#define TEST_SUITE_END() \
        printf("----------------------------------------\n"); \
        printf("Results: %d/%d passed", g_test_stats.passed, g_test_stats.total); \
        if (g_test_stats.failed > 0) { \
            printf(" (%d failed)" COLOR_RED " ✗" COLOR_RESET, g_test_stats.failed); \
        } else { \
            printf(COLOR_GREEN " ✓" COLOR_RESET); \
        } \
        printf("\n\n"); \
    }

#define RUN_TEST(name) \
    do { \
        printf("\nRunning test: %s...\n", #name); \
        test_##name(); \
    } while(0)

/* Main test runner */
#define TEST_MAIN_BEGIN() \
    int main(int argc, char *argv[]) { \
        (void)argc; (void)argv; \
        printf("========================================\n"); \
        printf("libixland Test Suite\n"); \
        printf("========================================\n");

#define TEST_MAIN_END() \
        printf("========================================\n"); \
        printf("Final Results: %d/%d passed", g_test_stats.passed, g_test_stats.total); \
        if (g_test_stats.failed > 0) { \
            printf(" (%d failed)\n" COLOR_RED "SOME TESTS FAILED" COLOR_RESET "\n", g_test_stats.failed); \
            return 1; \
        } else { \
            printf("\n" COLOR_GREEN "ALL TESTS PASSED" COLOR_RESET "\n"); \
            return 0; \
        } \
    }

/* Utility functions */
static inline void test_reset_stats(void) {
    g_test_stats.total = 0;
    g_test_stats.passed = 0;
    g_test_stats.failed = 0;
    g_test_stats.skipped = 0;
}

#endif /* IXLAND_TEST_H */
