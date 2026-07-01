.text
.global _start
_start:
    mov x0, sp
    sub sp, sp, #16
    mov x1, #41
    str x1, [sp, #8]
    ldr x0, [sp, #8]
    add sp, sp, #16
    mov x8, #93
    svc #0
