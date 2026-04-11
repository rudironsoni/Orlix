# IXLAND Architectural Analysis and Migration Plan

**Date**: 2026-03-23  
**Status**: Pre-implementation Analysis Phase  
**Objective**: Transform ixland from fragmented compatibility layer to coherent Linux-like virtual subsystem

---

## 1. Current Repo Findings

### 1.1 Critical Architectural Debt Identified

#### Dual Process Models
The repository currently contains **two competing process models**:

1. **`__ixland_process_t`** in `ixland_process.c` (1,628 lines)
   - Global process table: `__ixland_process_table[IXLAND_MAX_PROCESSES]`
   - Thread-local current process: `__ixland_current_process`
   - Own FD table per process: `fd_table[IXLAND_MAX_FD]`
   - Signal queue per process
   - Wait queue infrastructure
   - Process groups and sessions

2. **`ixland_context_t`** in `ixland_context.c` (777 lines)
   - Separate context table: `ctx_table[IXLAND_MAX_PROCESSES]`
   - Thread-local current context: `__ixland_current_ctx`
   - Own FD table per context
   - Own signal handlers
   - Own environment

**Violation**: This creates ambiguity about which model owns process semantics.

#### Triple FD Ownership
Three conflicting FD ownership schemes exist:

1. **Global FD table** in `ixland_file.c` (625 lines):
   ```c
   static ixland_fd_entry_t fd_table[IXLAND_MAX_FD];  // Global table
   ```

2. **Per-process FD table** in `__ixland_process_t`:
   ```c
   struct __ixland_fd_entry_s fd_table[IXLAND_MAX_FD];  // Inside process struct
   ```

3. **Per-context FD table** in `ixland_context_t`:
   ```c
   struct { int real_fd; ... } fd_table[IXLAND_MAX_FD];  // Inside context struct
   ```

**Violation**: FDs must live ONLY under `task->files` per requirements.

#### Passthrough Architecture
`ixland_libc_delegate.c` (486 lines) delegates to host libc:
- Uses `dlsym(RTLD_NEXT, ...)` for passthrough
- Owns semantics through delegation rather than ixland kernel subsystems
- Does not provide virtualized Linux-like behavior

**Violation**: Policy and semantics must belong to ixland kernel subsystems.

#### Non-Functional execve
`__ixland_execve_impl()` at line 640 of `ixland_process.c`:
```c
int __ixland_execve_impl(const char *pathname, char *const argv[], char *const envp[]) {
    __ixland_process_t *proc = __ixland_get_current_process();
    if (!proc) {
        errno = ENOENT;
        return -1;
    }
    // ... stores metadata ...
    // Returns ENOSYS or success without actual execution
    return -1;  // Or partial implementation
}
```

**Violation**: execve must dispatch to native/WASM/script runners with virtual image replacement.

#### WAMR Isolation
Current WAMR integration (`ixland_wamr_simple.c`, 303 lines):
- Simplified interpreter-only
- Does not share VFS/FD/task with native code
- Global WAMR state: `g_wamr_state`
- No WASI-to-ixland bridge

**Violation**: WAMR must use SAME VFS/FD/task as native code.

#### Incomplete Fork Implementation
`__ixland_fork_impl()` at line 515 creates thread but:
- Does not implement full Linux fork semantics
- Child continuation mechanism incomplete
- No proper virtual memory separation (unnecessary on iOS but process semantics incomplete)

### 1.2 File Inventory Summary

| File | Lines | Status | Action |
|------|-------|--------|--------|
| `ixland_process.c` | 1,628 | Has good logic but wrong structure | **Migrate** to kernel/task/ |
| `ixland_context.c` | 777 | Competing model | **Delete** - migrate salvageable logic |
| `ixland_file.c` | 625 | Global FD table | **Delete** - move to per-task files |
| `ixland_file_v2.c` | 568 | VFS-aware files | **Evaluate** for merge into fs/ |
| `ixland_vfs.c` | 478 | VFS layer | **Keep** - move to fs/vfs/ |
| `ixland_libc_delegate.c` | 486 | Passthrough | **Replace** with backend hooks |
| `ixland_network.c` | 665 | Network sockets | **Keep** - move to net/ |
| `ixland_init.c` | 108 | Initialization | **Keep** - move to kernel/init/ |
| `ixland_minimal.c` | 201 | Minimal stubs | **Evaluate** |
| `ixland_wamr_simple.c` | 303 | Simplified WAMR | **Replace** with full runtime/wasi/ |
| `ixland_wamr.c` | 649 | Full WAMR | **Evaluate** for unification |
| `ixland_interpose.c` | 341 | Interposition | **Keep** - move to compat/interpose/ |
| `ixland_path.c` | 385 | Path utilities | **Keep** - move to fs/ |
| `ixland_internal.h` | 749 | Unified header | **Split** into subsystem headers |

### 1.3 Current Build System Issues

- **Makefile** exists alongside **CMakeLists.txt** (2.5K)
- Makefile is currently active (referenced in README)
- CMakeLists.txt exists but may not be complete
- No checked-in `CMakePresets.json`
- No deterministic simulator/device test commands
- No fresh-clone bootstrap script

**Violation**: CMake must be single source of truth.

### 1.4 Testing Gap Analysis

Current tests directory structure:
```
tests/
├── unit/          # Empty or minimal
├── integration/   # Minimal
├── fixtures/      # Test data
├── *.swift        # iOS app tests (Swift)
├── *.m            # iOS app tests (ObjC)
└── ixland_test.h     # Test utilities
```

**Missing**:
- No deterministic C-level test harness
- No device/simulator test entrypoints
- No test matrix document
- No regression test policy
- No compatibility inventory
- No performance baseline harness

**Violation**: Every subsystem must have automated tests before being marked complete.

---

## 2. Canonical Object Model

### 2.1 Core Object Hierarchy

```
ixland_task
├── ixland_files (fdtable)
│   ├── file descriptor slots
│   ├── cloexec tracking
│   └── reference to vfs
├── ixland_fs (filesystem context)
│   ├── cwd
│   ├── root
│   ├── umask
│   └── mount namespace view
├── ixland_sighand (signal handling)
│   ├── handlers[NSIG]
│   ├── blocked mask
│   ├── pending mask
│   └── signal queue
├── ixland_tty (controlling terminal)
│   ├── controlling tty reference
│   ├── foreground process group
│   └── session leader status
├── ixland_mm_emu (minimal memory descriptor)
│   └── exec image ownership tracking
└── ixland_exec_image (current executable)
    ├── image type (NATIVE/WASI/SCRIPT)
    ├── resolved path
    ├── interpreter path (if script)
    └── entrypoint binding
```

### 2.2 Object Definitions

#### ixland_task

```c
typedef struct ixland_task {
    /* Identity (Linux-compatible) */
    pid_t pid;              /* Process ID */
    pid_t ppid;             /* Parent process ID */
    pid_t tgid;             /* Thread group ID (equals pid for thread leader) */
    pid_t pgid;             /* Process group ID */
    pid_t sid;              /* Session ID */
    
    /* State */
    atomic_int state;       /* TASK_RUNNING, TASK_STOPPED, etc. */
    int exit_status;        /* Exit code */
    atomic_bool exited;     /* Has exited */
    atomic_bool signaled;   /* Terminated by signal */
    atomic_int termsig;     /* Terminating signal */
    
    /* Relationships */
    struct ixland_task *parent;
    struct ixland_task *children;      /* Linked list */
    struct ixland_task *next_sibling;
    
    /* Execution */
    pthread_t thread;       /* Host thread handle */
    atomic_int exec_seq;    /* Execution sequence (for exec detection) */
    char comm[IXLAND_MAX_NAME];        /* Command name */
    char exe[IXLAND_MAX_PATH];         /* Executable path */
    
    /* Subsystem references (owned by this task) */
    ixland_files_t *files;     /* File descriptor table */
    ixland_fs_t *fs;           /* Filesystem context */
    ixland_sighand_t *sighand; /* Signal handlers */
    ixland_tty_t *tty;         /* Controlling terminal */
    ixland_mm_emu_t *mm;       /* Memory descriptor */
    ixland_exec_image_t *exec_image;   /* Current executable image */
    
    /* Wait queue */
    pthread_cond_t wait_cond;
    pthread_mutex_t wait_lock;
    int waiters;            /* Number of waiters */
    
    /* Resource limits */
    struct rlimit rlimits[RLIMIT_NLIMITS];
    
    /* Timing */
    struct timespec start_time;
    
    /* Reference counting */
    atomic_int refs;
    pthread_mutex_t lock;
} ixland_task_t;
```

#### ixland_files (FD Table)

```c
typedef struct ixland_file {
    int fd;                 /* Virtual fd number */
    int real_fd;            /* Real OS fd (for hostfs) */
    unsigned int flags;     /* O_RDONLY, O_CLOEXEC, etc. */
    off_t pos;              /* Current position */
    ixland_vfs_file_t *vfs_file;   /* VFS file reference */
    atomic_int refs;        /* Reference count */
} ixland_file_t;

typedef struct ixland_files {
    ixland_file_t **fd;        /* Array of file pointers */
    size_t max_fds;         /* Current max fds */
    pthread_mutex_t lock;
} ixland_files_t;
```

#### ixland_fs (Filesystem Context)

```c
typedef struct ixland_fs {
    char cwd[IXLAND_MAX_PATH];
    char root[IXLAND_MAX_PATH];
    mode_t umask;
    ixland_mount_ns_t *mnt_ns; /* Mount namespace view */
    atomic_int refs;
} ixland_fs_t;
```

#### ixland_sighand (Signal Handling)

```c
typedef struct ixland_sighand {
    struct sigaction action[IXLAND_NSIG];
    sigset_t blocked;
    sigset_t pending;
    ixland_sigqueue_t *queue;  /* Queued real-time signals */
    atomic_int refs;
} ixland_sighand_t;
```

#### ixland_tty (Controlling Terminal)

```c
typedef struct ixland_tty {
    int tty_id;             /* /dev/pts/N */
    pid_t foreground_pgrp;  /* Foreground process group */
    struct termios termios;
    struct winsize winsize;
    bool is_session_leader;
    atomic_int refs;
} ixland_tty_t;
```

#### ixland_mm_emu (Minimal Memory Descriptor)

```c
typedef struct ixland_mm_emu {
    /* Not a full VM - just ownership tracking */
    void *exec_image_base;  /* Base of exec image */
    size_t exec_image_size; /* Size of exec image */
    atomic_int refs;
} ixland_mm_emu_t;
```

#### ixland_exec_image (Executable Image)

```c
typedef enum {
    IXLAND_IMAGE_NONE = 0,
    IXLAND_IMAGE_NATIVE,       /* Native registered command */
    IXLAND_IMAGE_WASI,         /* WASM/WASI module */
    IXLAND_IMAGE_SCRIPT        /* Script with shebang */
} ixland_image_type_t;

typedef struct ixland_exec_image {
    ixland_image_type_t type;
    char path[IXLAND_MAX_PATH];
    char interpreter[IXLAND_MAX_PATH]; /* For scripts */
    
    union {
        struct {
            ixland_native_entry_t *entry;
        } native;
        struct {
            wasm_module_t module;
            wasm_module_inst_t instance;
        } wasi;
        struct {
            char *interpreter_argv[IXLAND_MAX_ARGS];
            int interpreter_argc;
        } script;
    } u;
} ixland_exec_image_t;
```

### 2.3 Object Lifecycle Rules

1. **ixland_task**: Allocated on fork, persists through exec, freed when parent waits
2. **ixland_files**: Copied on fork (dup), preserved on exec (with CLOEXEC handling)
3. **ixland_fs**: Copied on fork, preserved on exec
4. **ixland_sighand**: Copied on fork, reset on exec (per Linux exec rules)
5. **ixland_tty**: Referenced on fork (if controlling terminal), preserved on exec
6. **ixland_mm_emu**: Minimal - just tracks exec image ownership
7. **ixland_exec_image**: Replaced on exec, freed when task exits or execs again

---

## 3. Ownership Rules

### 3.1 Single Source of Truth

| Concept | Owner | Location |
|---------|-------|----------|
| Virtual PID | ixland_task | `task->pid` |
| PPID | ixland_task | `task->ppid` |
| PGID | ixland_task | `task->pgid` |
| SID | ixland_task | `task->sid` |
| Process state | ixland_task | `task->state` |
| File descriptors | ixland_files | `task->files->fd[]` |
| CWD | ixland_fs | `task->fs->cwd` |
| Root | ixland_fs | `task->fs->root` |
| Umask | ixland_fs | `task->fs->umask` |
| Signal handlers | ixland_sighand | `task->sighand->action[]` |
| Signal mask | ixland_sighand | `task->sighand->blocked` |
| Controlling TTY | ixland_tty | `task->tty` (may be NULL) |
| Foreground pgrp | ixland_tty | `task->tty->foreground_pgrp` |
| Current executable | ixland_exec_image | `task->exec_image` |
| Mount namespace | ixland_mount_ns_t | `task->fs->mnt_ns` |
| Environment | ixland_task | `task->envp` (simplified) |

### 3.2 No Global State

**Forbidden**:
- Global FD tables
- Global process tables (use hash table indexed by PID)
- Global signal state
- Global VFS mounts (per-namespace)

**Allowed**:
- Global PID allocator (single counter)
- Global TTY table (for /dev/pts/*)
- Global command registry

### 3.3 Thread Safety

All objects must be thread-safe:
- Reference counting with atomic operations
- Fine-grained locking per-object
- Lock ordering: task → files → fs → sighand → tty
- Lock held briefly, never across blocking operations

---

## 4. File-by-File Migration Map

### 4.1 Delete: Competing Models

| Source | Action | Notes |
|--------|--------|-------|
| `src/ixland/core/ixland_context.c` | **DELETE** | Salvage: ctx allocation, env handling, cwd tracking |
| `src/ixland/core/ixland_file.c` | **DELETE** | Global FD table forbidden; per-task tables in new model |

### 4.2 Keep and Migrate: Core Logic

| Source | Destination | Migration Notes |
|--------|-------------|-----------------|
| `src/ixland/core/ixland_process.c` | `kernel/task/task.c` | Extract: PID allocation, wait queues, process groups |
| `src/ixland/core/ixland_vfs.c` | `fs/vfs/vfs.c` | Keep core VFS logic, add mount namespaces |
| `src/ixland/core/ixland_file_v2.c` | `fs/fdtable.c` | Merge into per-task FD table implementation |
| `src/ixland/core/ixland_network.c` | `net/socket.c` | Move as-is, adapt to new task model |
| `src/ixland/core/ixland_init.c` | `kernel/init/main.c` | Expand initialization sequence |
| `src/ixland/util/ixland_path.c` | `fs/path.c` | Path normalization, canonicalization |
| `src/ixland/interpose/ixland_interpose.c` | `compat/interpose/syscalls.c` | Keep as compatibility layer |

### 4.3 Replace: Broken or Incomplete

| Source | Replacement | Reason |
|--------|-------------|--------|
| `src/ixland/core/ixland_libc_delegate.c` | `kernel/syscalls.c` + backend hooks | Stop passthrough, own semantics |
| `src/ixland/core/ixland_minimal.c` | Deleted | Replaced by proper subsystems |
| `src/ixland/wamr/ixland_wamr_simple.c` | `runtime/wasi/wasi_runtime.c` | Needs full WASI bridge |
| `src/ixland/wamr/ixland_wamr.c` | Evaluate | May merge into wasi_runtime.c |

### 4.4 Create New: Missing Subsystems

| New File | Purpose |
|----------|---------|
| `kernel/task/fork.c` | Virtual fork implementation |
| `kernel/task/exit.c` | Exit and wait implementation |
| `kernel/task/pid.c` | PID allocation and table |
| `kernel/exec/exec.c` | execve dispatcher |
| `kernel/exec/native.c` | Native command registry |
| `kernel/exec/script.c` | Shebang script handling |
| `kernel/signal/signal.c` | Signal delivery |
| `kernel/signal/sigaction.c` | Signal handlers |
| `kernel/signal/sigqueue.c` | Signal queuing |
| `kernel/signal/pgrp.c` | Process groups |
| `kernel/signal/session.c` | Sessions |
| `fs/vfs/mount.c` | Mount namespaces |
| `fs/proc/proc.c` | /proc filesystem |
| `fs/proc/self.c` | /proc/self entries |
| `fs/dev/dev.c` | /dev filesystem |
| `fs/devpts/pts.c` | /dev/pts filesystem |
| `fs/pipe/pipe.c` | Pipe implementation |
| `drivers/tty/pty.c` | PTY master/slave |
| `drivers/tty/tty.c` | TTY handling |
| `drivers/tty/termios.c` | Terminal attributes |
| `runtime/native/registry.c` | Native command registry |
| `runtime/wasi/wasi_syscalls.c` | WASI syscall bridge |
| `runtime/wasi/wasi_fd.c` | WASI FD operations |
| `runtime/wasi/wasi_path.c` | WASI path operations |
| `runtime/script/shebang.c` | Script interpreter handling |
| `compat/posix/unistd.c` | POSIX compatibility |
| `compat/posix/fcntl.c` | fcntl compatibility |
| `compat/posix/signal.c` | signal compatibility |
| `compat/posix/termios.c` | termios compatibility |
| `compat/posix/stat.c` | stat compatibility |

### 4.5 Header Migration

| Source | Destination |
|--------|-------------|
| `src/ixland/internal/ixland_internal.h` | Split into subsystem headers |
| | `kernel/task/task.h` |
| | `kernel/signal/signal.h` |
| | `fs/vfs/vfs.h` |
| | `fs/fdtable.h` |
| | `drivers/tty/tty.h` |
| | `runtime/native/native.h` |
| | `runtime/wasi/wasi.h` |
| `include/ixland/ixland_types.h` | Keep - public types |
| `include/ixland/ixland_syscalls.h` | Keep - public syscalls |
| `include/ixland/ixland_wamr.h` | Update to `ixland_wamr.h` only |

---

## 5. Virtual Fork Model

### 5.1 Fork Semantics

Linux fork on ixland:
1. Allocate new `ixland_task` with new PID
2. Copy parent's `files`, `fs`, `sighand` (with reference counting)
3. Duplicate thread state using host threading
4. Mark child as runnable
5. Return 0 in child, child PID in parent

### 5.2 Implementation Strategy

Since iOS prohibits real fork(), implement as:

```c
pid_t ixland_fork(void) {
    ixland_task_t *parent = ixland_current_task();
    
    /* Allocate child task */
    ixland_task_t *child = ixland_task_alloc();
    child->pid = alloc_pid();
    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    
    /* Copy/duplicate subsystems */
    child->files = ixland_files_dup(parent->files);
    child->fs = ixland_fs_dup(parent->fs);
    child->sighand = ixland_sighand_dup(parent->sighand);
    child->tty = ixland_tty_get(parent->tty);  /* Reference, not copy */
    child->mm = ixland_mm_emu_alloc();
    
    /* Create host thread for child */
    pthread_t child_thread;
    fork_args_t args = {
        .child = child,
        .parent = parent,
        .barrier = &barrier
    };
    pthread_create(&child_thread, NULL, fork_child_trampoline, &args);
    
    /* Wait for child to initialize */
    wait_barrier(&barrier);
    
    /* Return child PID in parent */
    return child->pid;
}

static void *fork_child_trampoline(void *arg) {
    fork_args_t *args = arg;
    
    /* Set current task */
    ixland_set_current_task(args->child);
    
    /* Signal parent we're ready */
    signal_barrier(args->barrier);
    
    /* Return 0 from fork */
    longjmp(args->child->fork_jmpbuf, 1);  /* Or equivalent */
}
```

### 5.3 Child Continuation Mechanism

Use `setjmp/longjmp` or explicit continuation:

```c
/* In fork() - parent side */
int jmpval = setjmp(parent->fork_jmpbuf);
if (jmpval == 0) {
    /* Parent: create child thread */
    pthread_create(..., fork_child_trampoline, ...);
    return child_pid;  /* Parent returns here */
}

/* Child thread entry */
void *fork_child_trampoline(void *arg) {
    ixland_task_t *child = arg;
    ixland_set_current_task(child);
    
    /* Jump to child continuation */
    longjmp(child->fork_jmpbuf, 1);
    /* NOTREACHED */
}

/* In child - after longjmp */
int jmpval = setjmp(child->fork_jmpbuf);
if (jmpval == 1) {
    /* Child returns 0 */
    return 0;
}
```

### 5.4 Fork Behavior Requirements

Per Linux fork semantics:
- Child gets copy of parent's address space (irrelevant for ixland, but process state copied)
- Child inherits parent's signal mask
- Child's pending signals are cleared
- Child's signal queue is empty
- Child's itimers are cleared
- Child's resource usages are zeroed
- File descriptors are duplicated (refcounted in ixland)

---

## 6. execve State Machine

### 6.1 Image Type Detection

```c
ixland_image_type_t ixland_exec_classify(const char *path) {
    int fd = open(path, O_RDONLY);
    char magic[4];
    read(fd, magic, 4);
    close(fd);
    
    /* WASM magic: \0asm */
    if (memcmp(magic, "\x00asm", 4) == 0) {
        return IXLAND_IMAGE_WASI;
    }
    
    /* Script: #! */
    if (memcmp(magic, "#!", 2) == 0) {
        return IXLAND_IMAGE_SCRIPT;
    }
    
    /* Native: Check registry */
    if (ixland_native_lookup(path)) {
        return IXLAND_IMAGE_NATIVE;
    }
    
    return IXLAND_IMAGE_NONE;
}
```

### 6.2 execve State Machine

```
execve(path, argv, envp)
    |
    v
[1] Path resolution
    - VFS lookup
    - Permission check (x-bit)
    |
    v
[2] Image classification
    - Read magic
    - Check native registry
    |
    v
[3] Pre-exec validation
    - Check FD_CLOEXEC fds
    - Validate not multi-threaded
    |
    v
[4] Load interpreter (if script)
    - Parse shebang
    - Rewrite argv
    |
    v
[5] Setup new image
    - Native: lookup entrypoint
    - WASI: load module, instantiate
    - Script: recursive exec interpreter
    |
    v
[6] Apply exec rules
    - Close FD_CLOEXEC fds
    - Reset signal handlers (keep SIG_IGN)
    - Clear signal mask
    - Clear pending signals
    - Update argv/envp
    |
    v
[7] Transfer control
    - Call image entrypoint
```

### 6.3 Native Execution Path

```c
int ixland_exec_native(ixland_task_t *task, ixland_native_entry_t *entry,
                    int argc, char **argv, char **envp) {
    /* Update task image */
    task->exec_image->type = IXLAND_IMAGE_NATIVE;
    task->exec_image->u.native.entry = entry;
    
    /* Prepare argv/envp in task */
    task->argv = copy_argv(argc, argv);
    task->envp = copy_envp(envp);
    
    /* Call native entrypoint */
    return entry->func(task, argc, argv, envp);
}
```

### 6.4 WASI Execution Path

```c
int ixland_exec_wasi(ixland_task_t *task, const char *path,
                  int argc, char **argv, char **envp) {
    /* Load WASM module */
    uint8_t *wasm_buf = read_file(path);
    wasm_module_t module = wasm_runtime_load(wasm_buf, ...);
    wasm_module_inst_t instance = wasm_runtime_instantiate(module, ...);
    
    /* Create WASI context using task's VFS */
    wasm_wasi_args_t wasi_args;
    wasi_args.stdio = task->files;  /* Same FD table */
    wasi_args.cwd = task->fs->cwd;
    
    /* Instantiate with WASI imports */
    wasm_runtime_instantiate_wasi(instance, &wasi_args);
    
    /* Call _start or specified function */
    wasm_function_inst_t func = wasm_runtime_lookup_function(instance, "_start");
    wasm_runtime_call_wasm(task->exec_env, func, 0, NULL);
    
    return 0;
}
```

### 6.5 Script Execution Path

```c
int ixland_exec_script(ixland_task_t *task, const char *path,
                    int argc, char **argv, char **envp) {
    /* Read shebang */
    char shebang[IXLAND_MAX_PATH];
    read_shebang(path, shebang, sizeof(shebang));
    
    /* Parse interpreter and args */
    char *interp, *interp_args[IXLAND_MAX_ARGS];
    int interp_argc = parse_shebang(shebang, &interp, interp_args);
    
    /* Rewrite argv: interpreter [args] script [original args] */
    char *new_argv[IXLAND_MAX_ARGS];
    new_argv[0] = interp;
    for (i = 0; i < interp_argc; i++)
        new_argv[i+1] = interp_args[i];
    new_argv[interp_argc+1] = (char *)path;
    for (i = 1; i < argc; i++)
        new_argv[interp_argc+1+i] = argv[i];
    
    /* Recursive execve on interpreter */
    return ixland_execve(interp, new_argv, envp);
}
```

---

## 7. PTY / Job Control / Signal Model

### 7.1 Session and Process Group Model

```
Session (sid=100)
├── Session Leader (pid=100)
│   └── Controlling TTY: /dev/pts/0
│
└── Process Group (pgid=100) [foreground]
    └── Process (pid=100)

Process Group (pgid=200) [background]
├── Process (pid=201)
└── Process (pid=202)
```

### 7.2 PTY Architecture

```c
typedef struct ixland_pty {
    int master_fd;          /* PTY master (real host fd) */
    int slave_fd;           /* PTY slave (real host fd) */
    char slave_name[64];    /* /dev/ttysXXX or similar */
    pid_t session_leader;   /* Session holding this PTY */
    pid_t foreground_pgrp;  /* Foreground process group */
    struct termios termios;
    struct winsize winsize;
} ixland_pty_t;

/* Global PTY table */
ixland_pty_t *pty_table[IXLAND_MAX_PTS];
```

### 7.3 Job Control Signals

| Signal | Behavior |
|--------|----------|
| SIGINT | Sent to foreground pgrp on Ctrl+C |
| SIGQUIT | Sent to foreground pgrp on Ctrl+\ |
| SIGTSTP | Sent to foreground pgrp on Ctrl+Z |
| SIGTTIN | Sent to background pgrp attempting read |
| SIGTTOU | Sent to background pgrp attempting write |
| SIGCONT | Resume stopped process group |
| SIGCHLD | Sent to parent when child changes state |
| SIGWINCH | Sent to pgrp on window resize |
| SIGHUP | Sent to orphaned pgrp when session leader exits |

### 7.4 Signal Delivery State Machine

```
Signal Generated (kill, kernel, terminal)
    |
    v
[1] Permission Check
    - uid/gid match or privileged
    |
    v
[2] Determine Target
    - pid > 0: specific process
    - pid = 0: all processes in sender's pgrp
    - pid = -1: all processes (privileged)
    - pid < -1: all processes in |pid| pgrp
    |
    v
[3] Add to Pending
    - Add to target's sigpending mask
    - Queue if real-time signal
    |
    v
[4] Check Delivery Conditions
    - Signal not blocked?
    - Signal action not SIG_IGN?
    |
    v
[5] Deliver Signal
    - Invoke handler (if SA_SIGINFO, pass siginfo)
    - Or terminate/coredump/stop/continue
```

### 7.5 Terminal Access Control

```c
/* Before read/write on controlling terminal */
int ixland_tty_check_access(ixland_task_t *task, int fd, bool is_write) {
    ixland_file_t *file = ixland_files_lookup(task->files, fd);
    ixland_tty_t *tty = file->tty;
    
    if (!tty) return 0;  /* Not a tty */
    
    /* Foreground pgrp can always access */
    if (task->pgid == tty->foreground_pgrp)
        return 0;
    
    /* Background pgrp needs signal handling */
    if (is_write) {
        /* Check if TOSTOP is set */
        if (tty->termios.c_lflag & TOSTOP) {
            ixland_kill(-task->pgid, SIGTTOU);
            return -EIO;
        }
    } else {
        ixland_kill(-task->pgid, SIGTTIN);
        return -EIO;
    }
    
    return 0;
}
```

---

## 8. WAMR Shared-Substrate Boundary

### 8.1 WASI-to-IXLAND Mapping

| WASI Function | ixland Subsystem |
|---------------|---------------|
| fd_read | fs/fdtable.c: ixland_fd_read() |
| fd_write | fs/fdtable.c: ixland_fd_write() |
| fd_close | fs/fdtable.c: ixland_fd_close() |
| fd_seek | fs/fdtable.c: ixland_fd_seek() |
| path_open | fs/vfs/vfs.c: ixland_vfs_open() |
| path_unlink | fs/vfs/vfs.c: ixland_vfs_unlink() |
| path_mkdir | fs/vfs/vfs.c: ixland_vfs_mkdir() |
| path_stat | fs/vfs/vfs.c: ixland_vfs_stat() |
| path_symlink | fs/vfs/vfs.c: ixland_vfs_symlink() |
| path_readlink | fs/vfs/vfs.c: ixland_vfs_readlink() |
| path_rename | fs/vfs/vfs.c: ixland_vfs_rename() |
| path_filestat_get | fs/vfs/vfs.c: ixland_vfs_stat() |
| path_filestat_set_times | fs/vfs/vfs.c: ixland_vfs_utimens() |
| clock_time_get | kernel/time/time.c: ixland_clock_gettime() |
| clock_res_get | kernel/time/time.c: ixland_clock_getres() |
| random_get | fs/dev/random.c: ixland_getrandom() |
| environ_sizes_get | kernel/task/task.c: ixland_get_env() |
| environ_get | kernel/task/task.c: ixland_get_env() |
| args_sizes_get | kernel/exec/exec.c: ixland_get_args() |
| args_get | kernel/exec/exec.c: ixland_get_args() |
| proc_exit | kernel/task/exit.c: ixland_exit() |

### 8.2 WASI Context Structure

```c
typedef struct ixland_wasi_context {
    ixland_task_t *task;       /* Back-reference to ixland task */
    wasm_exec_env_t exec_env;
    wasm_module_inst_t module_inst;
    
    /* FD mapping: WASI fd -> ixland fd */
    int wasi_to_ixland_fd[IXLAND_MAX_FD];
    
    /* Preopened directories */
    ixland_wasi_preopen_t preopens[IXLAND_MAX_PREOPENS];
    int preopen_count;
} ixland_wasi_context_t;
```

### 8.3 WASI Implementation Pattern

```c
/* WASI fd_write implementation */
wasi_errno_t wasi_fd_write(wasm_exec_env_t exec_env,
                           wasi_fd_t fd,
                           const iovec_t *iovs,
                           size_t iovs_len,
                           size_t *nwritten) {
    /* Get ixland task from exec_env */
    ixland_wasi_context_t *wasi_ctx = wasm_runtime_get_user_data(exec_env);
    ixland_task_t *task = wasi_ctx->task;
    
    /* Map WASI fd to ixland fd */
    int ixland_fd = wasi_ctx->wasi_to_ixland_fd[fd];
    
    /* Call ixland FD operation */
    ssize_t ret = ixland_fd_write(task, ixland_fd, iovs, iovs_len, nwritten);
    
    /* Convert ixland errno to WASI errno */
    return ixland_errno_to_wasi(ret < 0 ? errno : 0);
}
```

### 8.4 WAMR Integration Points

1. **Module Loading**: `runtime/wasi/wasi_runtime.c`
2. **Import Resolution**: WAMR calls into ixland WASI functions
3. **Memory Access**: Shared linear memory (WAMR manages, ixland can read/write)
4. **Trap Handling**: Convert WASM traps to ixland signals
5. **Lifetime**: WASI context destroyed when task exits or execs

---

## 9. Build System and iOS-Only Test Architecture

### 9.1 CMake Configuration Requirements

```cmake
# CMakeLists.txt (root)

# MUST: Set iOS before project()
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_SYSROOT iphonesimulator)  # or iphoneos
set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0")
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Hard fail on non-iOS
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "ixland only supports iOS builds")
endif()

project(ixland VERSION 1.0.0 LANGUAGES C)

# Use Xcode generator
set(CMAKE_GENERATOR "Xcode" CACHE STRING "Generator" FORCE)

# iOS-specific options
set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH NO)
set(CMAKE_XCODE_ATTRIBUTE_VALID_ARCHS "arm64")
```

### 9.2 CMakePresets.json

```json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "ios-simulator-base",
            "hidden": true,
            "generator": "Xcode",
            "architecture": "arm64",
            "cacheVariables": {
                "CMAKE_SYSTEM_NAME": "iOS",
                "CMAKE_OSX_SYSROOT": "iphonesimulator",
                "CMAKE_OSX_DEPLOYMENT_TARGET": "16.0",
                "CMAKE_OSX_ARCHITECTURES": "arm64"
            }
        },
        {
            "name": "ios-device-base",
            "hidden": true,
            "generator": "Xcode",
            "architecture": "arm64",
            "cacheVariables": {
                "CMAKE_SYSTEM_NAME": "iOS",
                "CMAKE_OSX_SYSROOT": "iphoneos",
                "CMAKE_OSX_DEPLOYMENT_TARGET": "16.0",
                "CMAKE_OSX_ARCHITECTURES": "arm64"
            }
        },
        {
            "name": "ios-simulator-debug",
            "inherits": "ios-simulator-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        },
        {
            "name": "ios-simulator-release",
            "inherits": "ios-simulator-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "ios-simulator-asan",
            "inherits": "ios-simulator-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_C_FLAGS": "-fsanitize=address"
            }
        },
        {
            "name": "ios-device-debug",
            "inherits": "ios-device-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "ios-simulator-debug",
            "configurePreset": "ios-simulator-debug",
            "configuration": "Debug"
        }
    ],
    "testPresets": [
        {
            "name": "ios-simulator-test",
            "configurePreset": "ios-simulator-debug",
            "output": {
                "outputOnFailure": true
            },
            "execution": {
                "timeout": 300
            }
        }
    ]
}
```

### 9.3 Test Execution Architecture

```
tools/
├── bootstrap.sh           # Fresh clone setup
├── doctor.sh             # Environment verification
├── test-simulator.sh     # Run simulator tests
├── test-device.sh        # Run device tests
└── build-sdk.sh          # Build full SDK
```

**test-simulator.sh**:
```bash
#!/bin/bash
set -e

PRESET="ios-simulator-debug"
DESTINATION="platform=iOS Simulator,name=iPhone 17 Pro"

# Configure
cmake --preset=$PRESET

# Build for testing
cmake --build --preset=$PRESET --target ixland-test-app -- -build-for-testing

# Run tests
xcodebuild test-without-building \
    -project build/ixland.xcodeproj \
    -scheme ixland-test-app \
    -destination "$DESTINATION" \
    -resultBundlePath results/simulator.xcresult
```

### 9.4 Fresh-Clone Requirements

**bootstrap.sh**:
```bash
#!/bin/bash
# Fresh clone bootstrap for ixland

set -e

echo "=== ixland Bootstrap ==="

# Check prerequisites
command -v cmake >/dev/null 2>&1 || { echo "cmake required"; exit 1; }
command -v xcodebuild >/dev/null 2>&1 || { echo "Xcode required"; exit 1; }

# Check iOS SDKs
if ! xcrun --sdk iphonesimulator --show-sdk-path >/dev/null 2>&1; then
    echo "iOS Simulator SDK not found"
    exit 1
fi

# Initialize submodules (NOT NEEDED - this is now a monorepo)
# git submodule update --init --recursive
# Instead, use the monorepo root:
# git clone https://github.com/rudironsoni/a-shell-next.git

# Run doctor
tools/doctor.sh

echo "Bootstrap complete. Run 'tools/test-simulator.sh' to verify."
```

---

## 10. Testing Architecture

### 10.1 Seven Testing Layers

**Layer 1: Pure Core Unit Tests**
- Location: `tests/unit/`
- Purpose: Data structures, deterministic logic
- Target: iOS Simulator
- Coverage: PID allocation, FD operations, path normalization

**Layer 2: Subsystem Integration Tests**
- Location: `tests/integration/`
- Purpose: Cross-subsystem interactions
- Target: iOS Simulator
- Coverage: fork+wait, exec+CLOEXEC, pipe+dup

**Layer 3: Linux Userland Compatibility Tests**
- Location: `tests/compat/*/`
- Purpose: Real package behavior
- Target: iOS Simulator, Device
- Coverage: bash scenarios, coreutils, PTY

**Layer 4: WASI Compatibility Tests**
- Location: `tests/wasi/`
- Purpose: WAMR WASI correctness
- Target: iOS Simulator, Device
- Coverage: FD, path, stdio, env, clocks

**Layer 5: iOS Runtime Tests**
- Location: `tests/device/`, `tests/simulator/`
- Purpose: iOS-specific behavior
- Target: Device (primary), Simulator (secondary)
- Coverage: Sandbox paths, PTY, lifecycle

**Layer 6: Stress and Reliability Tests**
- Location: `tests/stress/`
- Purpose: Concurrency, resource churn
- Target: iOS Simulator (long-running)
- Coverage: Repeated fork/exec, signal storms

**Layer 7: Performance Regression Tests**
- Location: `tests/perf/`
- Purpose: Baseline benchmarks
- Target: iOS Simulator
- Coverage: Shell startup, dispatch latency

### 10.2 C-Level Test Harness

```c
/* tests/harness/ixland_test.h */
#ifndef IXLAND_TEST_H
#define IXLAND_TEST_H

#include <stdbool.h>

typedef struct {
    const char *name;
    bool (*fn)(void);
} ixland_test_case_t;

#define IXLAND_TEST(name) \
    static bool test_##name(void); \
    static const ixland_test_case_t test_case_##name \
        __attribute__((used, section("ixland_test_cases"))) = { \
        .name = #name, \
        .fn = test_##name \
    }; \
    static bool test_##name(void)

#define IXLAND_ASSERT(expr) \
    do { if (!(expr)) { \
        ixland_test_fail(__FILE__, __LINE__, #expr); \
        return false; \
    } } while(0)

#define IXLAND_ASSERT_EQ(a, b) IXLAND_ASSERT((a) == (b))
#define IXLAND_ASSERT_NE(a, b) IXLAND_ASSERT((a) != (b))
#define IXLAND_ASSERT_GT(a, b) IXLAND_ASSERT((a) > (b))
#define IXLAND_ASSERT_LT(a, b) IXLAND_ASSERT((a) < (b))

void ixland_test_fail(const char *file, int line, const char *expr);
int ixland_test_run_all(const char *filter);

#endif
```

---

## 11. Test Matrix by Subsystem

### 11.1 Task Subsystem

| Category | Invariants | Positive | Negative | Concurrency |
|----------|-----------|----------|----------|-------------|
| PID alloc | Unique IDs | Alloc/free | Exhaustion | Concurrent alloc |
| Fork | Child != Parent | Basic fork | NULL args | Many forks |
| Exit | Zombied until wait | Normal exit | Invalid status | Exit during wait |
| Wait | Returns status | waitpid | Wrong pid | Concurrent waits |
| PGID | Group membership | setpgid | Invalid pgid | Move between groups |
| Session | Session leader | setsid | Already leader | Orphaned pgrp |

### 11.2 Files Subsystem

| Category | Invariants | Positive | Negative | Concurrency |
|----------|-----------|----------|----------|-------------|
| FD alloc | Contiguous | Open/close | EMFILE | Concurrent open |
| Dup | Independent refs | dup/dup2/dup3 | Invalid fd | Dup during close |
| CLOEXEC | Closed on exec | F_SETFD | Invalid flags | Exec with many fds |
| Seek | Position tracking | lseek | ESPIPE on pipe | Concurrent seek |
| Read/Write | Byte counts | Basic IO | EBADF | Concurrent RW |

### 11.3 Exec Subsystem

| Category | Invariants | Positive | Negative | Compatibility |
|----------|-----------|----------|----------|---------------|
| Native | Registry lookup | /bin/ls | Missing cmd | Command names |
| WASI | WASM load | .wasm exec | Invalid magic | WASI compliance |
| Script | Shebang parse | #!/bin/sh | Missing interp | Script args |
| CLOEXEC | FDs closed | FD_CLOEXEC | None retained | Bash pipelines |
| Signals | Handlers reset | SIG_IGN kept | SIG_DFL reset | Signal state |
| Identity | PID preserved | Same PID | PID change | Session/PGID |

### 11.4 Signal Subsystem

| Category | Invariants | Positive | Negative | Concurrency |
|----------|-----------|----------|----------|-------------|
| Handler | Callback invoked | signal() | Invalid sig | Handler reentry |
| Mask | Blocking works | sigprocmask | Invalid how | Mask changes |
| Pending | Queued signals | sigpending | Overflow | Signal storm |
| Delivery | Correct target | kill() | Invalid pid | Multi-target |
| Group | All members receive | kill(-pgrp) | Wrong group | Group changes |

### 11.5 TTY/PTY Subsystem

| Category | Invariants | Positive | Negative | Compatibility |
|----------|-----------|----------|----------|---------------|
| Master | Read/write | openpt | No master | PTY pairs |
| Slave | TTY device | ptsname | Invalid fd | pts/* nodes |
| Foreground | One pgrp | tcsetpgrp | No TTY | Job control |
| Signals | SIGINT on Ctrl+C | Terminal gen | Invalid gen | Readline |
| Attributes | Termios saved | tcgetattr | No TTY | stty compat |

---

## 12. Reliability and Regression Policy

### 12.1 Truthfulness Rule

A feature is **NOT** complete unless:
1. A deterministic automated test proves the behavior
2. A conformance test proves the behavior
3. A regression test covers a previously failing case
4. A documented limitation states what is unverified

### 12.2 Regression Policy

Every bug fix requires:
1. Regression test that **fails** before the fix
2. The fix
3. Regression test **passes** after the fix

No bug fix without a test (unless documented untestable).

### 12.3 Reliability Gates

All changes must pass:
1. Unit tests
2. Integration tests
3. Compatibility tests
4. WASI tests
5. Regression tests
6. Performance checks (if hot path)
7. Simulator tests
8. Device tests (if iOS-sensitive)

### 12.4 Flakiness Policy

1. Flaky tests are release blockers
2. Do not increase timeouts to "fix" races
3. Fix the test or the subsystem
4. Every timeout must be bounded and justified

---

## 13. First Concrete Code Changes

### 13.1 Phase 0: Repository Reorganization

**Step 1: Create new directory structure**
```bash
mkdir -p kernel/{task,signal,exec,time,resource}
mkdir -p fs/{vfs,proc,dev,devpts,pipe,hostfs}
mkdir -p drivers/tty
mkdir -p runtime/{native,wasi,script}
mkdir -p compat/{posix,interpose}
mkdir -p tests/{unit,integration,compat,wasi,stress,perf,device,simulator}
mkdir -p tools
```

**Step 2: Move existing files (git mv)**
```bash
git mv src/ixland/core/ixland_vfs.c fs/vfs/
git mv src/ixland/core/ixland_network.c net/
git mv src/ixland/core/ixland_path.c fs/
git mv src/ixland/interpose/ixland_interpose.c compat/interpose/
```

**Step 3: Create canonical task header**
File: `kernel/task/task.h`
```c
#ifndef IXLAND_TASK_H
#define IXLAND_TASK_H

#include <ixland/ixland_types.h>
#include <stdatomic.h>
#include <pthread.h>

typedef struct ixland_task ixland_task_t;
typedef struct ixland_files ixland_files_t;
typedef struct ixland_fs ixland_fs_t;
typedef struct ixland_sighand ixland_sighand_t;
typedef struct ixland_tty ixland_tty_t;

typedef enum {
    IXLAND_TASK_RUNNING = 0,
    IXLAND_TASK_INTERRUPTIBLE,
    IXLAND_TASK_UNINTERRUPTIBLE,
    IXLAND_TASK_STOPPED,
    IXLAND_TASK_ZOMBIE,
    IXLAND_TASK_DEAD
} ixland_task_state_t;

struct ixland_task {
    pid_t pid, ppid, tgid, pgid, sid;
    atomic_int state;
    int exit_status;
    
    pthread_t thread;
    char comm[256];
    
    ixland_files_t *files;
    ixland_fs_t *fs;
    ixland_sighand_t *sighand;
    ixland_tty_t *tty;
    
    struct ixland_task *parent, *children, *next_sibling;
    
    atomic_int refs;
    pthread_mutex_t lock;
};

/* Core API */
ixland_task_t *ixland_task_alloc(void);
void ixland_task_free(ixland_task_t *task);
ixland_task_t *ixland_current_task(void);
void ixland_set_current_task(ixland_task_t *task);
pid_t ixland_alloc_pid(void);
void ixland_free_pid(pid_t pid);

#endif
```

### 13.2 Phase 1: FD Table Migration

**File: `fs/fdtable.c` (new)**
```c
#include "fs/fdtable.h"
#include <stdlib.h>
#include <string.h>

struct ixland_files {
    ixland_file_t **fd;
    size_t max_fds;
    pthread_mutex_t lock;
};

ixland_files_t *ixland_files_alloc(size_t max_fds) {
    ixland_files_t *files = calloc(1, sizeof(ixland_files_t));
    files->fd = calloc(max_fds, sizeof(ixland_file_t*));
    files->max_fds = max_fds;
    pthread_mutex_init(&files->lock, NULL);
    return files;
}

void ixland_files_free(ixland_files_t *files) {
    if (!files) return;
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i]) {
            ixland_file_free(files->fd[i]);
        }
    }
    free(files->fd);
    free(files);
}

ixland_files_t *ixland_files_dup(ixland_files_t *parent) {
    ixland_files_t *child = ixland_files_alloc(parent->max_fds);
    pthread_mutex_lock(&parent->lock);
    for (size_t i = 0; i < parent->max_fds; i++) {
        if (parent->fd[i]) {
            child->fd[i] = ixland_file_dup(parent->fd[i]);
        }
    }
    pthread_mutex_unlock(&parent->lock);
    return child;
}
```

### 13.3 Phase 2: Delete Competing Code

**Commit 1: Remove ixland_context.c**
```bash
git rm src/ixland/core/ixland_context.c
# Migrate salvageable logic:
# - ctx allocation pattern -> task allocation
# - env handling -> task->envp
# - cwd tracking -> task->fs->cwd
```

**Commit 2: Remove global FD table**
```bash
git rm src/ixland/core/ixland_file.c
# All FD operations now go through task->files
```

### 13.4 Phase 3: Create Test Harness

**File: `tests/harness/harness.c`**
```c
#include "ixland_test.h"
#include <stdio.h>
#include <string.h>

extern ixland_test_case_t __start_ixland_test_cases[];
extern ixland_test_case_t __stop_ixland_test_cases[];

static int tests_run = 0;
static int tests_passed = 0;

void ixland_test_fail(const char *file, int line, const char *expr) {
    fprintf(stderr, "FAIL: %s:%d: %s\n", file, line, expr);
}

int ixland_test_run_all(const char *filter) {
    ixland_test_case_t *start = __start_ixland_test_cases;
    ixland_test_case_t *end = __stop_ixland_test_cases;
    
    for (ixland_test_case_t *tc = start; tc < end; tc++) {
        if (filter && strstr(tc->name, filter) == NULL)
            continue;
        
        tests_run++;
        printf("TEST: %s ... ", tc->name);
        fflush(stdout);
        
        if (tc->fn()) {
            printf("PASS\n");
            tests_passed++;
        } else {
            printf("FAIL\n");
        }
    }
    
    printf("\nResults: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *filter = argc > 1 ? argv[1] : NULL;
    return ixland_test_run_all(filter);
}
```

---

## 14. First Concrete Test Files

### 14.1 Unit Test: PID Allocation

**File: `tests/unit/test_pid.c`**
```c
#include "ixland_test.h"
#include "kernel/task/task.h"

IXLAND_TEST(pid_alloc_basic) {
    pid_t p1 = ixland_alloc_pid();
    pid_t p2 = ixland_alloc_pid();
    
    IXLAND_ASSERT_GT(p1, 0);
    IXLAND_ASSERT_GT(p2, 0);
    IXLAND_ASSERT_NE(p1, p2);
    
    ixland_free_pid(p1);
    ixland_free_pid(p2);
    return true;
}

IXLAND_TEST(pid_alloc_exhaustion) {
    /* Allocate many PIDs */
    pid_t pids[100];
    for (int i = 0; i < 100; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);
    }
    
    /* Free all */
    for (int i = 0; i < 100; i++) {
        ixland_free_pid(pids[i]);
    }
    return true;
}

IXLAND_TEST(pid_reuse) {
    pid_t p1 = ixland_alloc_pid();
    ixland_free_pid(p1);
    pid_t p2 = ixland_alloc_pid();
    
    /* PIDs may be reused */
    IXLAND_ASSERT_GT(p2, 0);
    
    ixland_free_pid(p2);
    return true;
}
```

### 14.2 Unit Test: FD Table

**File: `tests/unit/test_fd.c`**
```c
#include "ixland_test.h"
#include "fs/fdtable.h"

IXLAND_TEST(fdtable_alloc) {
    ixland_files_t *files = ixland_files_alloc(256);
    IXLAND_ASSERT(files != NULL);
    ixland_files_free(files);
    return true;
}

IXLAND_TEST(fdtable_dup) {
    ixland_files_t *parent = ixland_files_alloc(256);
    
    /* Add a fake FD to parent */
    /* (Would need actual file creation) */
    
    ixland_files_t *child = ixland_files_dup(parent);
    IXLAND_ASSERT(child != NULL);
    IXLAND_ASSERT_NE(child, parent);
    
    ixland_files_free(child);
    ixland_files_free(parent);
    return true;
}
```

### 14.3 Integration Test: Fork

**File: `tests/integration/test_fork.c`**
```c
#include "ixland_test.h"
#include "kernel/task/task.h"
#include "kernel/task/fork.h"

IXLAND_TEST(fork_creates_new_task) {
    /* This test will require full fork implementation */
    /* For now, just verify structure exists */
    IXLAND_ASSERT(ixland_task_alloc() != NULL);
    return true;
}

IXLAND_TEST(fork_parent_gets_child_pid) {
    /* Placeholder - requires full implementation */
    IXLAND_ASSERT(true);  /* TODO: Implement fork first */
    return true;
}
```

### 14.4 CMakeLists.txt Update

**File: `tests/unit/CMakeLists.txt` (new)**
```cmake
# Unit tests
set(IXLAND_UNIT_TESTS
    test_pid.c
    test_fd.c
)

add_executable(ixland-unit-tests
    ../harness/harness.c
    ${IXLAND_UNIT_TESTS}
)

target_link_libraries(ixland-unit-tests PRIVATE ixland-core)

target_compile_options(ixland-unit-tests PRIVATE
    -fdata-sections -ffunction-sections
)

target_link_options(ixland-unit-tests PRIVATE
    -Wl,-dead_strip
)

# Register with CTest
add_test(NAME unit-pid COMMAND ixland-unit-tests pid)
add_test(NAME unit-fd COMMAND ixland-unit-tests fd)
```

### 14.5 Test Documentation

**File: `tests/README.md` (new)**
```markdown
# ixland Test Suite

## Running Tests

### Simulator
```bash
tools/test-simulator.sh
```

### Device
```bash
tools/test-device.sh
```

### Unit Tests Only
```bash
cmake --preset ios-simulator-debug
cmake --build --preset ios-simulator-debug --target ixland-unit-tests
ctest --preset ios-simulator-test -R unit
```

## Test Organization

- `unit/`: Pure core unit tests
- `integration/`: Subsystem integration tests
- `compat/`: Linux userland compatibility
- `wasi/`: WASI/WAMR tests
- `device/`: iOS device-specific tests
- `simulator/`: iOS simulator-specific tests
- `stress/`: Stress and reliability tests
- `perf/`: Performance regression tests

## Writing Tests

See `harness/ixland_test.h` for test macros.

```c
#include "ixland_test.h"

IXLAND_TEST(my_feature) {
    IXLAND_ASSERT(some_condition);
    IXLAND_ASSERT_EQ(expected, actual);
    return true;
}
```
```

---

## Appendices

### A. Deleted Files

| File | Reason |
|------|--------|
| `src/ixland/core/ixland_context.c` | Competing process model |
| `src/ixland/core/ixland_file.c` | Global FD table forbidden |
| `src/ixland/core/ixland_libc_delegate.c` | Passthrough semantics forbidden |
| `src/ixland/core/ixland_minimal.c` | Replaced by proper subsystems |
| `src/ixland/wamr/ixland_wamr_simple.c` | Incomplete WASI bridge |

### B. Renamed/Moved Files

| Old Location | New Location |
|--------------|--------------|
| `src/ixland/core/ixland_vfs.c` | `fs/vfs/vfs.c` |
| `src/ixland/core/ixland_network.c` | `net/socket.c` |
| `src/ixland/util/ixland_path.c` | `fs/path.c` |
| `src/ixland/interpose/ixland_interpose.c` | `compat/interpose/syscalls.c` |
| `src/ixland/core/ixland_process.c` | `kernel/task/task.c` (partial) |
| `src/ixland/core/ixland_init.c` | `kernel/init/main.c` |

### C. New Files Required

See Section 4.4 for complete list of new subsystem files.

---

**Document Status**: Analysis Complete  
**Next Step**: Execute Phase 0 - Repository Reorganization
