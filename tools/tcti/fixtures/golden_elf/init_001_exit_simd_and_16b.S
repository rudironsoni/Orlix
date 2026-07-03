.text
.global _start
_start:
    .inst 0x4e211c01    // and v1.16b, v0.16b, v1.16b
    mov x0, #42
    mov x8, #93
    svc #0
