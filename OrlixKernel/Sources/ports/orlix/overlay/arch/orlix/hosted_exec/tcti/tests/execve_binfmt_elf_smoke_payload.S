/* SPDX-License-Identifier: GPL-2.0-only */

.text
.global _start
_start:
	mov	x0, #1
	adr	x1, marker
	mov	x2, #23
	mov	x8, #64
	svc	#0
	mov	x0, #0
	mov	x8, #93
	svc	#0

marker:
	.ascii	"ORLIX-USERLAND-TCTI-OK\n"
