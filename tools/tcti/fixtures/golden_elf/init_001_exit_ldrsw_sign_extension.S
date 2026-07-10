.text
.global _start
_start:
    adr x2, ldrsw_values
    .inst 0xb9801848
    add x0, x8, #93
    mov x8, #93
    svc #0

.section .rodata
.balign 4
ldrsw_values:
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0xffffffcd
