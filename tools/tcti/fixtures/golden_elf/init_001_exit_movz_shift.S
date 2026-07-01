.text
.global _start
_start:
    movz x8, #93, lsl #16
    mov x0, #42
    svc #0
