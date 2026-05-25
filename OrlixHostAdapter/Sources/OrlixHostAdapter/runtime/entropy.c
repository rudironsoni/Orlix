#include "OrlixHostAdapter/runtime/entropy.h"
#include "OrlixHostAdapter/runtime/host_tls.h"

#include <stdlib.h>

__attribute__((visibility("hidden"))) unsigned long orlix_host_entropy_read(
    void *buffer,
    unsigned long length)
{
    unsigned long active_tls;

    if (!buffer || !length) {
        return 0;
    }

    active_tls = OrlixHostEnterHostTls();
    arc4random_buf(buffer, (size_t)length);
    OrlixHostLeaveHostTls(active_tls);
    return length;
}
