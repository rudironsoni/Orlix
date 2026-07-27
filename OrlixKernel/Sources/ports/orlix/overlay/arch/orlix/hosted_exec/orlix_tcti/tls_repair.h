/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TLS_REPAIR_H
#define ORLIX_TCTI_TLS_REPAIR_H

#include <internal/asm/host_trap.h>

int orlix_tcti_decide_user_tls_repair(
	const struct orlix_host_user_tls_repair_request *request,
	struct orlix_host_user_tls_repair_decision *decision);

#endif /* ORLIX_TCTI_TLS_REPAIR_H */
