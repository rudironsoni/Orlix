.text
.global _start
_start:
    .inst 0x4f000421 // movi v1.4s, #0x1
    mov x0, #42
    mov x8, #93
    svc #0
