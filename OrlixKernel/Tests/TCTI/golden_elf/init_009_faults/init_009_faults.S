.text
.global _start
_start:
    mov x1, #0
    ldr x0, [x1]
    mov x0, #1
    mov x8, #93
    svc #0
