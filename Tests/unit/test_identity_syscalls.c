/* Unit tests for identity syscalls
 * Tests: ixland_getuid, ixland_geteuid, ixland_setuid, ixland_getgid, ixland_getegid, ixland_setgid
 */

#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#include "../../src/ixland/internal/ixland_internal.h"
#include "../harness/ixland_test.h"

IXLAND_TEST(getuid_returns_consistent_uid) {
    uid_t uid1 = ixland_getuid();
    uid_t uid2 = ixland_getuid();
    uid_t uid3 = ixland_getuid();

    /* All calls should return the same UID */
    IXLAND_ASSERT_EQ(uid1, uid2);
    IXLAND_ASSERT_EQ(uid2, uid3);
    IXLAND_ASSERT_GT(uid1, 0);

    return true;
}

IXLAND_TEST(getuid_returns_ios_default) {
    uid_t uid = ixland_getuid();

    /* On iOS, default UID is typically 501 (mobile user) */
    /* We allow either 0 (root) or 501 (mobile) as valid iOS UIDs */
    IXLAND_ASSERT(uid == 0 || uid == 501);

    return true;
}

IXLAND_TEST(geteuid_returns_consistent_euid) {
    uid_t euid1 = ixland_geteuid();
    uid_t euid2 = ixland_geteuid();

    /* All calls should return the same EUID */
    IXLAND_ASSERT_EQ(euid1, euid2);
    IXLAND_ASSERT_GT(euid1, 0);

    return true;
}

IXLAND_TEST(geteuid_matches_getuid_on_ios) {
    uid_t uid = ixland_getuid();
    uid_t euid = ixland_geteuid();

    /* On iOS, effective UID should match real UID */
    IXLAND_ASSERT_EQ(uid, euid);

    return true;
}

IXLAND_TEST(setuid_returns_eperm) {
    /* On iOS, setuid should always fail with EPERM */
    int result = ixland_setuid(0);

    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    /* Try with another UID */
    errno = 0;
    result = ixland_setuid(501);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    return true;
}

IXLAND_TEST(getgid_returns_consistent_gid) {
    gid_t gid1 = ixland_getgid();
    gid_t gid2 = ixland_getgid();
    gid_t gid3 = ixland_getgid();

    /* All calls should return the same GID */
    IXLAND_ASSERT_EQ(gid1, gid2);
    IXLAND_ASSERT_EQ(gid2, gid3);
    IXLAND_ASSERT_GT(gid1, 0);

    return true;
}

IXLAND_TEST(getgid_returns_ios_default) {
    gid_t gid = ixland_getgid();

    /* On iOS, default GID is typically 501 (mobile group) */
    /* We allow either 0 (wheel) or 501 (mobile) as valid iOS GIDs */
    IXLAND_ASSERT(gid == 0 || gid == 501 || gid == 20);

    return true;
}

IXLAND_TEST(getegid_returns_consistent_egid) {
    gid_t egid1 = ixland_getegid();
    gid_t egid2 = ixland_getegid();

    /* All calls should return the same EGID */
    IXLAND_ASSERT_EQ(egid1, egid2);
    IXLAND_ASSERT_GT(egid1, 0);

    return true;
}

IXLAND_TEST(getegid_matches_getgid_on_ios) {
    gid_t gid = ixland_getgid();
    gid_t egid = ixland_getegid();

    /* On iOS, effective GID should match real GID */
    IXLAND_ASSERT_EQ(gid, egid);

    return true;
}

IXLAND_TEST(setgid_returns_eperm) {
    /* On iOS, setgid should always fail with EPERM */
    int result = ixland_setgid(0);

    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    /* Try with another GID */
    errno = 0;
    result = ixland_setgid(501);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    return true;
}

IXLAND_TEST(identity_values_consistent_across_calls) {
    /* Verify that multiple identity syscalls return consistent values */
    uid_t uid1 = ixland_getuid();
    gid_t gid1 = ixland_getgid();

    for (int i = 0; i < 100; i++) {
        uid_t uid2 = ixland_getuid();
        gid_t gid2 = ixland_getgid();

        IXLAND_ASSERT_EQ(uid1, uid2);
        IXLAND_ASSERT_EQ(gid1, gid2);
    }

    return true;
}

IXLAND_TEST(identity_syscalls_no_side_effects) {
    /* Calling get functions should not affect errno */
    errno = 0;

    ixland_getuid();
    IXLAND_ASSERT_EQ(errno, 0);

    ixland_geteuid();
    IXLAND_ASSERT_EQ(errno, 0);

    ixland_getgid();
    IXLAND_ASSERT_EQ(errno, 0);

    ixland_getegid();
    IXLAND_ASSERT_EQ(errno, 0);

    return true;
}
