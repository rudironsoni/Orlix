/* iXland - Identity Syscalls
 *
 * Canonical owner for user/group identity syscalls:
 * - getuid(), geteuid(), getgid(), getegid()
 * - setuid(), setgid()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 * iOS apps run as a single user (mobile/501) and cannot change identity.
 */

#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/* ============================================================================
 * USER/GROUP IDENTITY - Private Implementation
 * ============================================================================ */

static uid_t (*libc_getuid)(void) = NULL;
static uid_t (*libc_geteuid)(void) = NULL;
static gid_t (*libc_getgid)(void) = NULL;
static gid_t (*libc_getegid)(void) = NULL;

static uid_t getuid_impl(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_getuid = (uid_t (*)(void))dlsym(RTLD_NEXT, "getuid");
    }
    if (libc_getuid) {
        return libc_getuid();
    }
    /* Return iOS default uid (mobile user) */
    return 501;
}

static uid_t geteuid_impl(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_geteuid = (uid_t (*)(void))dlsym(RTLD_NEXT, "geteuid");
    }
    if (libc_geteuid) {
        return libc_geteuid();
    }
    /* On iOS, effective UID is same as real UID */
    return getuid_impl();
}

static gid_t getgid_impl(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_getgid = (gid_t (*)(void))dlsym(RTLD_NEXT, "getgid");
    }
    if (libc_getgid) {
        return libc_getgid();
    }
    /* Return iOS default gid (mobile group) */
    return 501;
}

static gid_t getegid_impl(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_getegid = (gid_t (*)(void))dlsym(RTLD_NEXT, "getegid");
    }
    if (libc_getegid) {
        return libc_getegid();
    }
    /* On iOS, effective GID is same as real GID */
    return getgid_impl();
}

static int setuid_impl(uid_t uid) {
    (void)uid;
    /* On iOS, apps cannot change UID - always return EPERM */
    errno = EPERM;
    return -1;
}

static int setgid_impl(gid_t gid) {
    (void)gid;
    /* On iOS, apps cannot change GID - always return EPERM */
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) uid_t getuid(void) {
    return getuid_impl();
}

__attribute__((visibility("default"))) uid_t geteuid(void) {
    return geteuid_impl();
}

__attribute__((visibility("default"))) gid_t getgid(void) {
    return getgid_impl();
}

__attribute__((visibility("default"))) gid_t getegid(void) {
    return getegid_impl();
}

__attribute__((visibility("default"))) int setuid(uid_t uid) {
    return setuid_impl(uid);
}

__attribute__((visibility("default"))) int setgid(gid_t gid) {
    return setgid_impl(gid);
}
