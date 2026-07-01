.text
.global _start
_start:
    mov x8, #93
    mov x0, #42
    .inst 0xd503201f
    svc #0
