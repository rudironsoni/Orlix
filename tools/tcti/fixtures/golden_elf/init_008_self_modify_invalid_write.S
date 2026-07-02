.text
.global _start
_start:
    mov x1, #0
    mov x0, #42
    str x0, [x1]
    mov x0, #0
    mov x8, #93
    svc #0
