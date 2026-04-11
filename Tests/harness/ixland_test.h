#ifndef IXLAND_TEST_H
#define IXLAND_TEST_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    bool (*fn)(void);
} ixland_test_case_t;

#define IXLAND_TEST_SECTION_NAME "ix_test_cases"

#ifdef __APPLE__
#define IXLAND_SECTION __attribute__((used, section("__DATA," IXLAND_TEST_SECTION_NAME)))
#define IXLAND_TEST_CASE_START __start_ix_test_cases
#define IXLAND_TEST_CASE_STOP __stop_ix_test_cases
#else
#define IXLAND_SECTION __attribute__((used, section(IXLAND_TEST_SECTION_NAME)))
#define IXLAND_TEST_CASE_START __start_ix_test_cases
#define IXLAND_TEST_CASE_STOP __stop_ix_test_cases
#endif

#define IXLAND_TEST(name)                                                                   \
    static bool test_##name(void);                                                          \
    static const ixland_test_case_t test_case_##name IXLAND_SECTION = {#name, test_##name}; \
    static bool test_##name(void)

#define IXLAND_ASSERT(expr)                              \
    do {                                                 \
        if (!(expr)) {                                   \
            ixland_test_fail(__FILE__, __LINE__, #expr); \
            return false;                                \
        }                                                \
    } while (0)

#define IXLAND_ASSERT_EQ(a, b) IXLAND_ASSERT((a) == (b))
#define IXLAND_ASSERT_NE(a, b) IXLAND_ASSERT((a) != (b))
#define IXLAND_ASSERT_GT(a, b) IXLAND_ASSERT((a) > (b))
#define IXLAND_ASSERT_LT(a, b) IXLAND_ASSERT((a) < (b))
#define IXLAND_ASSERT_GE(a, b) IXLAND_ASSERT((a) >= (b))
#define IXLAND_ASSERT_LE(a, b) IXLAND_ASSERT((a) <= (b))
#define IXLAND_ASSERT_NULL(p) IXLAND_ASSERT((p) == NULL)
#define IXLAND_ASSERT_NOT_NULL(p) IXLAND_ASSERT((p) != NULL)
#define IXLAND_ASSERT_TRUE(expr) IXLAND_ASSERT((expr))
#define IXLAND_ASSERT_FALSE(expr) IXLAND_ASSERT(!(expr))

void ixland_test_fail(const char *file, int line, const char *expr);
int ixland_test_run_all(const char *filter);
extern ixland_test_case_t IXLAND_TEST_CASE_START[];
extern ixland_test_case_t IXLAND_TEST_CASE_STOP[];

#ifdef __cplusplus
}
#endif

#endif
