#ifndef IXLAND_OBSERVABILITY_H
#define IXLAND_OBSERVABILITY_H

#ifdef __cplusplus
extern "C" {
#endif

void ixland_observability_emit_linux_compat_event(const char *event_name);

#ifdef __cplusplus
}
#endif

#endif