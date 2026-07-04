.text
.global _start
_start:
    .inst 0x6f07e7e0 // movi v0.2d, #0xffffffffffffffff
    mov x0, #42
    mov x8, #93
    svc #0
