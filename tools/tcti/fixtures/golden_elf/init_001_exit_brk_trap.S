.text
.global _start
_start:
    .inst 0xd4200020    // brk #0x1
    mov x0, #42
    mov x8, #93
    svc #0
