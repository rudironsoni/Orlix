.text
.global _start
_start:
    .inst 0x0e612800 // xtn v0.4h, v0.4s
    mov x0, #42
    mov x8, #93
    svc #0
