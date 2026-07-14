void orlix_tcti_forbidden_fixture(void) {
    unsigned int orlix_hosted_syscall_gate_page[4];
    int flags = MAP_JIT;
    int perms = PROT_READ | PROT_WRITE | PROT_EXEC;
    (void)mmap(0, 4096, perms, flags, -1, 0);
    (void)mmap(0, 4096, PROT_EXEC, 0, -1, 0);
    (void)vm_protect(0, 0, 4096, 0, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
    orlix_hosted_syscall_gate_page[0] = 0xd4000001u;
    (void)orlix_host_user_map_trusted_executable_page(orlix_hosted_syscall_gate_page);
    /* guest text must never receive VM_PROT_EXECUTE. */
    /* guest Linux must not receive HostAdapter or UIKit API surfaces. */
}
