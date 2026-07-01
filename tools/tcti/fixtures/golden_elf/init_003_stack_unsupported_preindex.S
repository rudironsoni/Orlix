.text
.global _start
_start:
    sub sp, sp, #16
    mov x1, #42
    str x1, [sp, #8]!
    ldr x0, [sp]
    add sp, sp, #16
    mov x8, #93
    svc #0
