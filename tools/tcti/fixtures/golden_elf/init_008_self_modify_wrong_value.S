.text
.global _start
_start:
    adr x1, patch_slot
    mov x0, #41
    str x0, [x1]
    mov x0, #0
    mov x8, #93
    svc #0
patch_slot:
    .quad 0
