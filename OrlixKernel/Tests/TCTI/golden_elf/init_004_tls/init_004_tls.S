.text
.global _start
_start:
    mov x1, #42
    msr tpidr_el0, x1
    mov x0, #0
    mrs x0, tpidr_el0
    mov x8, #93
    svc #0
