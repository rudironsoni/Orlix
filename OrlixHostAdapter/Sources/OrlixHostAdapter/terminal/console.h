#ifndef ORLIX_HOST_ADAPTER_TERMINAL_CONSOLE_H
#define ORLIX_HOST_ADAPTER_TERMINAL_CONSOLE_H

/* App-private HostAdapter SPI; not kernel-visible Linux contract. */
enum orlix_host_console_source {
    ORLIX_HOST_CONSOLE_SOURCE_SERIAL = 0,
    ORLIX_HOST_CONSOLE_SOURCE_VIRTIO = 1,
};

__attribute__((visibility("default"))) void
orlix_host_console_set_output_fd(enum orlix_host_console_source source, int fd);
__attribute__((visibility("default"))) unsigned long
orlix_host_console_enqueue_input(enum orlix_host_console_source source,
                                 const void *bytes, unsigned long length);
__attribute__((visibility("default"))) void
orlix_host_console_clear_input(enum orlix_host_console_source source);
__attribute__((visibility("default"))) void
orlix_host_console_recent_output_clear(enum orlix_host_console_source source);
__attribute__((visibility("default"))) unsigned long
orlix_host_console_recent_output_snapshot(enum orlix_host_console_source source,
                                          void *bytes,
                                          unsigned long capacity);
__attribute__((visibility("hidden"))) void
orlix_host_console_write(enum orlix_host_console_source source,
                         const void *bytes,
                         unsigned long length);
__attribute__((visibility("hidden"))) unsigned long
orlix_host_console_read_input(enum orlix_host_console_source source,
                              void *bytes, unsigned long length);

#endif
