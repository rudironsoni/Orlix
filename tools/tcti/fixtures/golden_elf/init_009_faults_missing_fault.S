.text
.global _start
_start:
    adr x1, value
    ldr x0, [x1]
    mov x8, #93
    svc #0
value:
    .quad 1
