.text
.global _start
_start:
    adr x0, page
    mov x1, #4096
    mov x2, #1
    mov x8, #227
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0
.p2align 12
page:
    .quad 0
