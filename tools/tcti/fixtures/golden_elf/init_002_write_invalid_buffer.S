.text
.global _start
_start:
    mov x0, #1
    mov x1, #0
    mov x2, #6
    mov x8, #64
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0
msg:
    .ascii "hello\n"
