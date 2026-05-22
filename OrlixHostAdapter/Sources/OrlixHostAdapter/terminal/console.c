#include "OrlixHostAdapter/terminal/console.h"

#include <limits.h>
#include <os/log.h>
#include <stddef.h>
#include <unistd.h>

static os_log_t OrlixHostConsoleLog(void)
{
    static os_log_t log;

    if (!log) {
        log = os_log_create("org.orlix.OrlixTerminal", "kernel");
    }

    return log;
}

__attribute__((visibility("hidden"))) void orlix_host_console_write(
    const char *bytes,
    unsigned long length)
{
    unsigned long offset;

    if (!bytes || length == 0) {
        return;
    }

    (void)write(STDERR_FILENO, bytes, (size_t)length);

    for (offset = 0; offset < length;) {
        unsigned long remaining = length - offset;
        unsigned long chunk = remaining > 1024 ? 1024 : remaining;
        int chunk_length = chunk > (unsigned long)INT_MAX ? INT_MAX : (int)chunk;

        os_log_with_type(OrlixHostConsoleLog(),
                         OS_LOG_TYPE_INFO,
                         "%{public}.*s",
                         chunk_length,
                         bytes + offset);
        offset += (unsigned long)chunk_length;
    }
}
