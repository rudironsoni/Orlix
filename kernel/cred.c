/* iOS Subsystem for Linux - Identity Syscalls
 *
 * Implementation of user/group identity syscalls.
 * On iOS, apps run as a single user (mobile/501) and cannot change identity.
 */

#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/* ============================================================================
 * USER/GROUP IDENTITY
 * ============================================================================ */

static uid_t (*libc_getuid)(void) = NULL;
static uid_t (*libc_geteuid)(void) = NULL;
static gid_t (*libc_getgid)(void) = NULL;
static gid_t (*libc_getegid)(void) = NULL;

/**
 * @brief Get real user ID
 *
 * Returns the real user ID of the calling process.
 * On iOS, this is typically 501 (mobile user).
 *
 * @return uid_t User ID
 */
uid_t ixland_getuid(void) {
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

/**
 * @brief Get effective user ID
 *
 * Returns the effective user ID of the calling process.
 * On iOS, effective UID is the same as real UID.
 *
 * @return uid_t Effective user ID
 */
uid_t ixland_geteuid(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_geteuid = (uid_t (*)(void))dlsym(RTLD_NEXT, "geteuid");
    }
    if (libc_geteuid) {
        return libc_geteuid();
    }
    /* On iOS, effective UID is same as real UID */
    return ixland_getuid();
}

/**
 * @brief Get real group ID
 *
 * Returns the real group ID of the calling process.
 * On iOS, this is typically 501 (mobile group).
 *
 * @return gid_t Group ID
 */
gid_t ixland_getgid(void) {
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

/**
 * @brief Get effective group ID
 *
 * Returns the effective group ID of the calling process.
 * On iOS, effective GID is the same as real GID.
 *
 * @return gid_t Effective group ID
 */
gid_t ixland_getegid(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        libc_getegid = (gid_t (*)(void))dlsym(RTLD_NEXT, "getegid");
    }
    if (libc_getegid) {
        return libc_getegid();
    }
    /* On iOS, effective GID is same as real GID */
    return ixland_getgid();
}

/**
 * @brief Set real and effective user ID
 *
 * Attempts to set the real and effective user ID.
 * On iOS, apps cannot change UID - always returns EPERM.
 *
 * @param uid New user ID
 * @return int 0 on success, -1 on error with errno set
 */
int ixland_setuid(uid_t uid) {
    (void)uid;
    /* On iOS, apps cannot change UID - always return EPERM */
    errno = EPERM;
    return -1;
}

/**
 * @brief Set real and effective group ID
 *
 * Attempts to set the real and effective group ID.
 * On iOS, apps cannot change GID - always returns EPERM.
 *
 * @param gid New group ID
 * @return int 0 on success, -1 on error with errno set
 */
int ixland_setgid(gid_t gid) {
    (void)gid;
    /* On iOS, apps cannot change GID - always return EPERM */
    errno = EPERM;
    return -1;
}
