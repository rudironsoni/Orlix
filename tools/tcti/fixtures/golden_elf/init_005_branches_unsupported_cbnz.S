.text
.global _start
_start:
    mov x0, #1
    cbnz x0, 1f
    mov x0, #0
1:
    mov x8, #93
    svc #0
