.text
.global _start
_start:
    .inst 0x4f06e7e0    // movi v0.16b, #0xdf
    mov x0, #42
    mov x8, #93
    svc #0
