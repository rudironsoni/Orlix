.text
.global _start
_start:
    adrp x8, :got:payload_byte
    ldr x8, [x8, :got_lo12:payload_byte]
    ldrb w0, [x8]
    mov x8, #93
    svc #0

.section .rodata, "a", @progbits
.balign 4096
payload_byte:
    .byte 42
