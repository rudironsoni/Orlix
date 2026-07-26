/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_HOSTED_TLS_REPAIR_H
#define _ASM_ORLIX_HOSTED_TLS_REPAIR_H

#include <internal/asm/host_trap.h>

int orlix_hosted_decide_user_tls_repair(
	const struct orlix_host_user_tls_repair_request *request,
	struct orlix_host_user_tls_repair_decision *decision);

#endif /* _ASM_ORLIX_HOSTED_TLS_REPAIR_H */
