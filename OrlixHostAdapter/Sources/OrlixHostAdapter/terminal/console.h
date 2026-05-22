#ifndef ORLIX_HOST_ADAPTER_TERMINAL_CONSOLE_H
#define ORLIX_HOST_ADAPTER_TERMINAL_CONSOLE_H

__attribute__((visibility("hidden"))) void orlix_host_console_write(
    const char *bytes,
    unsigned long length);

#endif
