.text
.global _start
_start:
	.inst 0x4f000420 // movi v0.4s, #0x1
	.inst 0x4eb1b800 // addv s0, v0.4s
	mov x0, #42
	mov x8, #93
	svc #0
