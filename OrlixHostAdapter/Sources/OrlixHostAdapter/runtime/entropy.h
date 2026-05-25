#ifndef ORLIX_HOST_ADAPTER_RUNTIME_ENTROPY_H
#define ORLIX_HOST_ADAPTER_RUNTIME_ENTROPY_H

__attribute__((visibility("hidden"))) unsigned long orlix_host_entropy_read(
    void *buffer,
    unsigned long length);

#endif
