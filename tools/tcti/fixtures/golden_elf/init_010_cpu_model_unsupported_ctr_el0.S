.text
.global _start
_start:
    mrs x0, ctr_el0
    mov x8, #93
    svc #0
