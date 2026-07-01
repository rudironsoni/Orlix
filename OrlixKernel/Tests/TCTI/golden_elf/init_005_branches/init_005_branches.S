.text
.global _start
_start:
    mov x0, #0
    cbz x0, 1f
    mov x0, #1
    b 2f
1:
    mov x0, #42
2:
    mov x8, #93
    svc #0
