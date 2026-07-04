.text
.global _start
_start:
	.inst 0x4f000424 // movi v4.4s, #0x1
	.inst 0x6eb0a885 // umaxv s5, v4.4s
	mov x0, #42
	mov x8, #93
	svc #0
