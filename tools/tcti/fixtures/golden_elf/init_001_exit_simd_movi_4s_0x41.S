.text
.global _start
_start:
    .inst 0x4f020420 // movi v0.4s, #0x41
    mov x0, #42
    mov x8, #93
    svc #0
