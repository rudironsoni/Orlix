.text
.global _start
_start:
    .inst 0x4f011600    // orr v0.4s, #0x30
    mov x0, #42
    mov x8, #93
    svc #0
