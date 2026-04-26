/* IXLandSystemTests/HostTestSupport.c
 * Host-side test support helpers - added wrappers for host file ops
 *
 * This narrow containment implements the limited host helpers used by
 * host bridge Objective-C tests. It is the only file that may include
 * internal/ios bridging headers for access to host_*_impl symbols.
 */

#include "HostTestSupport.h"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

/* Include the broad internal backing IO header only in this containment
 * translation unit. Tests must not include it directly. */
#include "internal/ios/fs/backing_io.h"

int ixland_test_host_open(const char *path, int flags, unsigned int mode) {
    return host_open_impl(path, flags, mode);
}

int ixland_test_host_close(int fd) {
    return host_close_impl(fd);
}
