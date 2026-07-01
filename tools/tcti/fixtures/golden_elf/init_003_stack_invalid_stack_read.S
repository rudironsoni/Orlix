.text
.global _start
_start:
    sub sp, sp, #16
    ldr x0, [sp, #8]
    add sp, sp, #16
    mov x8, #93
    svc #0
