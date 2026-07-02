.text
.global _start
_start:
    adr x1, cpu_model
    mov x0, #0
    mov x8, #93
    svc #0

cpu_model:
    .ascii "orlix-aarch64-v2\n"
