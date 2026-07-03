.text
.global _start
_start:
    .inst 0x6e144401    // mov v1.s[2], v0.s[2]
    mov x0, #42
    mov x8, #93
    svc #0
