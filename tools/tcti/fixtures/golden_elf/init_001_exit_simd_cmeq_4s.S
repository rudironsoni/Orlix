.text
.global _start
_start:
	.inst 0x4f000421 // movi v1.4s, #0x1
	.inst 0x4f000423 // movi v3.4s, #0x1
	.inst 0x6ea18c64 // cmeq v4.4s, v3.4s, v1.4s
	mov x0, #42
	mov x8, #93
	svc #0
