.text
.global _start
_start:
    mov x1, #42
    mrs x0, tpidrro_el0
    mov x8, #93
    svc #0
