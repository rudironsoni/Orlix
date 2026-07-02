void orlix_tcti_forbidden_fixture(void) {
    int flags = MAP_JIT;
    int perms = PROT_READ | PROT_WRITE | PROT_EXEC;
    (void)mmap(0, 4096, perms, flags, -1, 0);
    (void)mmap(0, 4096, PROT_EXEC, 0, -1, 0);
    (void)vm_protect(0, 0, 4096, 0, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
    /* guest text must never receive VM_PROT_EXECUTE. */
    /* guest Linux must not receive HostAdapter or UIKit API surfaces. */
}
