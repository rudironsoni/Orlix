.text
.global _start
_start:
    .inst 0x4f000425    // movi v5.4s, #0x1
    .inst 0x1e2600ab    // fmov w11, s5
    mov x0, #42
    mov x8, #93
    svc #0
