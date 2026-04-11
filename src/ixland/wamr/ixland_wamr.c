/* iOS Subsystem for Linux - WAMR Integration
 *
 * WebAssembly Micro Runtime integration for running WASM binaries on iOS
 * Implements WASI syscall bridge to ixland
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

/* WAMR includes (from submodule) */
#include "../../../deps/wamr/core/iwasm/include/wasm_export.h"
#include "../../../deps/wamr/core/shared/platform/include/platform_wasi_types.h"

/* ============================================================================
 * WAMR STATE
 * ============================================================================ */

static wasm_module_t current_module = NULL;
static wasm_module_inst_t current_instance = NULL;
static wasm_exec_env_t current_exec_env = NULL;
static NativeSymbol *wasi_symbols = NULL;
static uint32_t wasi_symbol_count = 0;

/* ============================================================================
 * WASI SYSCALL BRIDGE
 * ============================================================================ */

/* WASI file descriptor structure */
typedef struct wasi_fd_entry {
    int real_fd;
    int wasi_fd;
    char path[1024];
    bool is_preopened;
    bool is_dir;
} wasi_fd_entry_t;

#define WASI_MAX_FD 128
static wasi_fd_entry_t wasi_fd_table[WASI_MAX_FD];
static int wasi_next_fd = 3;

/* WASI errno conversion */
static __wasi_errno_t errno_to_wasi(int err) {
    switch (err) {
    case 0:
        return __WASI_ESUCCESS;
    case EPERM:
        return __WASI_EPERM;
    case ENOENT:
        return __WASI_ENOENT;
    case EIO:
        return __WASI_EIO;
    case EBADF:
        return __WASI_EBADF;
    case ENOMEM:
        return __WASI_ENOMEM;
    case EACCES:
        return __WASI_EACCES;
    case EEXIST:
        return __WASI_EEXIST;
    case ENOTDIR:
        return __WASI_ENOTDIR;
    case EISDIR:
        return __WASI_EISDIR;
    case EINVAL:
        return __WASI_EINVAL;
    case EMFILE:
        return __WASI_EMFILE;
    case EFBIG:
        return __WASI_EFBIG;
    case ENOSPC:
        return __WASI_ENOSPC;
    case ESPIPE:
        return __WASI_ESPIPE;
    case EROFS:
        return __WASI_EROFS;
    case ENAMETOOLONG:
        return __WASI_ENAMETOOLONG;
    default:
        return __WASI_EINVAL;
    }
}

/* WASI: args_get - Get command line arguments */
static int32_t wasi_args_get(void *exec_env, int32_t argv_offset, int32_t argv_buf_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    /* Get memory for argv array */
    uint32_t *argv = wasm_runtime_addr_app_to_native(module_inst, argv_offset);
    char *argv_buf = wasm_runtime_addr_app_to_native(module_inst, argv_buf_offset);

    if (!argv || !argv_buf) {
        return __WASI_EFAULT;
    }

    /* Get current context */
    ixland_context_t *ctx = __ixland_current_ctx;
    if (!ctx) {
        /* No context - return empty args */
        argv[0] = argv_buf_offset;
        argv_buf[0] = '\0';
        return __WASI_ESUCCESS;
    }

    /* Copy arguments */
    uint32_t buf_offset = 0;
    pthread_mutex_lock(&ctx->env_lock);

    /* argv[0] is program name */
    argv[0] = argv_buf_offset + buf_offset;
    strncpy(argv_buf + buf_offset, ctx->name, 1023);
    buf_offset += strlen(ctx->name) + 1;

    pthread_mutex_unlock(&ctx->env_lock);

    return __WASI_ESUCCESS;
}

/* WASI: args_sizes_get - Get argc and argv buffer size */
static int32_t wasi_args_sizes_get(void *exec_env, int32_t argc_offset,
                                   int32_t argv_buf_size_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    uint32_t *argc = wasm_runtime_addr_app_to_native(module_inst, argc_offset);
    uint32_t *argv_buf_size = wasm_runtime_addr_app_to_native(module_inst, argv_buf_size_offset);

    if (!argc || !argv_buf_size) {
        return __WASI_EFAULT;
    }

    ixland_context_t *ctx = __ixland_current_ctx;
    if (!ctx) {
        *argc = 1;
        *argv_buf_size = 1;
    } else {
        *argc = 1; /* Just program name */
        *argv_buf_size = strlen(ctx->name) + 1;
    }

    return __WASI_ESUCCESS;
}

/* WASI: environ_get - Get environment variables */
static int32_t wasi_environ_get(void *exec_env, int32_t environ_offset,
                                int32_t environ_buf_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    uint32_t *environ = wasm_runtime_addr_app_to_native(module_inst, environ_offset);
    char *environ_buf = wasm_runtime_addr_app_to_native(module_inst, environ_buf_offset);

    if (!environ || !environ_buf) {
        return __WASI_EFAULT;
    }

    ixland_context_t *ctx = __ixland_current_ctx;
    if (!ctx) {
        environ[0] = environ_buf_offset;
        environ_buf[0] = '\0';
        return __WASI_ESUCCESS;
    }

    pthread_mutex_lock(&ctx->env_lock);

    uint32_t buf_offset = 0;
    for (int i = 0; i < ctx->env_count && ctx->env[i]; i++) {
        environ[i] = environ_buf_offset + buf_offset;
        size_t len = strlen(ctx->env[i]);
        memcpy(environ_buf + buf_offset, ctx->env[i], len + 1);
        buf_offset += len + 1;
    }

    pthread_mutex_unlock(&ctx->env_lock);

    return __WASI_ESUCCESS;
}

/* WASI: environ_sizes_get - Get environment count and buffer size */
static int32_t wasi_environ_sizes_get(void *exec_env, int32_t environ_count_offset,
                                      int32_t environ_buf_size_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    uint32_t *environ_count = wasm_runtime_addr_app_to_native(module_inst, environ_count_offset);
    uint32_t *environ_buf_size =
        wasm_runtime_addr_app_to_native(module_inst, environ_buf_size_offset);

    if (!environ_count || !environ_buf_size) {
        return __WASI_EFAULT;
    }

    ixland_context_t *ctx = __ixland_current_ctx;
    if (!ctx) {
        *environ_count = 0;
        *environ_buf_size = 0;
    } else {
        pthread_mutex_lock(&ctx->env_lock);

        *environ_count = ctx->env_count;
        *environ_buf_size = 0;

        for (int i = 0; i < ctx->env_count && ctx->env[i]; i++) {
            *environ_buf_size += strlen(ctx->env[i]) + 1;
        }

        pthread_mutex_unlock(&ctx->env_lock);
    }

    return __WASI_ESUCCESS;
}

/* WASI: fd_write - Write to file descriptor */
static int32_t wasi_fd_write(void *exec_env, int32_t fd, int32_t iovs_offset, int32_t iovs_len,
                             int32_t nwritten_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    __wasi_ciovec_t *iovs = wasm_runtime_addr_app_to_native(module_inst, iovs_offset);
    uint32_t *nwritten = wasm_runtime_addr_app_to_native(module_inst, nwritten_offset);

    if (!iovs || !nwritten) {
        return __WASI_EFAULT;
    }

    /* Map WASI fd to real fd */
    int real_fd = fd;
    if (fd >= 0 && fd < WASI_MAX_FD && wasi_fd_table[fd].real_fd >= 0) {
        real_fd = wasi_fd_table[fd].real_fd;
    }

    /* Write all iovs */
    size_t total_written = 0;
    for (int i = 0; i < iovs_len; i++) {
        void *buf = wasm_runtime_addr_app_to_native(module_inst, iovs[i].buf);
        if (!buf) {
            return __WASI_EFAULT;
        }

        ssize_t written = write(real_fd, buf, iovs[i].buf_len);
        if (written < 0) {
            return errno_to_wasi(errno);
        }

        total_written += written;

        if ((size_t)written < iovs[i].buf_len) {
            /* Short write */
            break;
        }
    }

    *nwritten = total_written;
    return __WASI_ESUCCESS;
}

/* WASI: fd_read - Read from file descriptor */
static int32_t wasi_fd_read(void *exec_env, int32_t fd, int32_t iovs_offset, int32_t iovs_len,
                            int32_t nread_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    __wasi_iovec_t *iovs = wasm_runtime_addr_app_to_native(module_inst, iovs_offset);
    uint32_t *nread = wasm_runtime_addr_app_to_native(module_inst, nread_offset);

    if (!iovs || !nread) {
        return __WASI_EFAULT;
    }

    /* Map WASI fd to real fd */
    int real_fd = fd;
    if (fd >= 0 && fd < WASI_MAX_FD && wasi_fd_table[fd].real_fd >= 0) {
        real_fd = wasi_fd_table[fd].real_fd;
    }

    /* Read into iovs */
    size_t total_read = 0;
    for (int i = 0; i < iovs_len; i++) {
        void *buf = wasm_runtime_addr_app_to_native(module_inst, iovs[i].buf);
        if (!buf) {
            return __WASI_EFAULT;
        }

        ssize_t bytes = read(real_fd, buf, iovs[i].buf_len);
        if (bytes < 0) {
            return errno_to_wasi(errno);
        }

        total_read += bytes;

        if (bytes == 0) {
            /* EOF */
            break;
        }
    }

    *nread = total_read;
    return __WASI_ESUCCESS;
}

/* WASI: fd_close - Close file descriptor */
static int32_t wasi_fd_close(void *exec_env, int32_t fd) {
    if (fd < 0 || fd >= WASI_MAX_FD) {
        return __WASI_EBADF;
    }

    if (wasi_fd_table[fd].real_fd < 0) {
        return __WASI_EBADF;
    }

    int ret = close(wasi_fd_table[fd].real_fd);
    if (ret < 0) {
        return errno_to_wasi(errno);
    }

    wasi_fd_table[fd].real_fd = -1;
    wasi_fd_table[fd].wasi_fd = -1;

    return __WASI_ESUCCESS;
}

/* WASI: fd_seek - Seek in file */
static int32_t wasi_fd_seek(void *exec_env, int32_t fd, int64_t offset, int32_t whence,
                            int32_t newoffset_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    uint64_t *newoffset = wasm_runtime_addr_app_to_native(module_inst, newoffset_offset);
    if (!newoffset) {
        return __WASI_EFAULT;
    }

    /* Map WASI fd to real fd */
    int real_fd = fd;
    if (fd >= 0 && fd < WASI_MAX_FD && wasi_fd_table[fd].real_fd >= 0) {
        real_fd = wasi_fd_table[fd].real_fd;
    }

    /* Convert WASI whence to native */
    int native_whence;
    switch (whence) {
    case __WASI_WHENCE_SET:
        native_whence = SEEK_SET;
        break;
    case __WASI_WHENCE_CUR:
        native_whence = SEEK_CUR;
        break;
    case __WASI_WHENCE_END:
        native_whence = SEEK_END;
        break;
    default:
        return __WASI_EINVAL;
    }

    off_t result = lseek(real_fd, offset, native_whence);
    if (result < 0) {
        return errno_to_wasi(errno);
    }

    *newoffset = result;
    return __WASI_ESUCCESS;
}

/* WASI: path_open - Open file at path */
static int32_t wasi_path_open(void *exec_env, int32_t dirfd, int32_t dirflags, int32_t path_offset,
                              int32_t path_len, int32_t oflags, int64_t fs_rights_base,
                              int64_t fs_rights_inheriting, int32_t fdflags, int32_t fd_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    const char *path = wasm_runtime_addr_app_to_native(module_inst, path_offset);
    uint32_t *fd = wasm_runtime_addr_app_to_native(module_inst, fd_offset);

    if (!path || !fd) {
        return __WASI_EFAULT;
    }

    /* Convert WASI flags to native */
    int native_flags = O_CLOEXEC;

    if (oflags & __WASI_O_CREAT)
        native_flags |= O_CREAT;
    if (oflags & __WASI_O_DIRECTORY)
        native_flags |= O_DIRECTORY;
    if (oflags & __WASI_O_EXCL)
        native_flags |= O_EXCL;
    if (oflags & __WASI_O_TRUNC)
        native_flags |= O_TRUNC;

    if (fs_rights_base & __WASI_RIGHT_FD_READ)
        native_flags |= O_RDONLY;
    if (fs_rights_base & __WASI_RIGHT_FD_WRITE) {
        if (native_flags & O_RDONLY) {
            native_flags = (native_flags & ~O_RDONLY) | O_RDWR;
        } else {
            native_flags |= O_WRONLY;
        }
    }

    /* Open through VFS */
    int real_fd = ixland_vfs_open(path, native_flags, 0644);
    if (real_fd < 0) {
        return errno_to_wasi(errno);
    }

    /* Allocate WASI fd */
    int wasi_fd = -1;
    for (int i = 3; i < WASI_MAX_FD; i++) {
        if (wasi_fd_table[i].real_fd < 0) {
            wasi_fd = i;
            break;
        }
    }

    if (wasi_fd < 0) {
        close(real_fd);
        return __WASI_EMFILE;
    }

    wasi_fd_table[wasi_fd].real_fd = real_fd;
    wasi_fd_table[wasi_fd].wasi_fd = wasi_fd;
    strncpy(wasi_fd_table[wasi_fd].path, path, sizeof(wasi_fd_table[wasi_fd].path) - 1);

    *fd = wasi_fd;
    return __WASI_ESUCCESS;
}

/* WASI: proc_exit - Exit process */
static void wasi_proc_exit(void *exec_env, int32_t rval) {
    ixland_exit_full(rval);
}

/* WASI: clock_time_get - Get clock time */
static int32_t wasi_clock_time_get(void *exec_env, int32_t clock_id, int64_t precision,
                                   int32_t time_offset) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return __WASI_EINVAL;
    }

    uint64_t *time = wasm_runtime_addr_app_to_native(module_inst, time_offset);
    if (!time) {
        return __WASI_EFAULT;
    }

    struct timespec ts;
    clockid_t native_clock;

    switch (clock_id) {
    case __WASI_CLOCK_REALTIME:
        native_clock = CLOCK_REALTIME;
        break;
    case __WASI_CLOCK_MONOTONIC:
        native_clock = CLOCK_MONOTONIC;
        break;
    case __WASI_CLOCK_PROCESS_CPUTIME_ID:
        native_clock = CLOCK_PROCESS_CPUTIME_ID;
        break;
    case __WASI_CLOCK_THREAD_CPUTIME_ID:
        native_clock = CLOCK_THREAD_CPUTIME_ID;
        break;
    default:
        return __WASI_EINVAL;
    }

    if (clock_gettime(native_clock, &ts) < 0) {
        return errno_to_wasi(errno);
    }

    *time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return __WASI_ESUCCESS;
}

/* ============================================================================
 * NATIVE SYMBOL TABLE
 * ============================================================================ */

static NativeSymbol wasi_native_symbols[] = {
    {"args_get", wasi_args_get, "(ii)i"},
    {"args_sizes_get", wasi_args_sizes_get, "(ii)i"},
    {"environ_get", wasi_environ_get, "(ii)i"},
    {"environ_sizes_get", wasi_environ_sizes_get, "(ii)i"},
    {"fd_write", wasi_fd_write, "(iiii)i"},
    {"fd_read", wasi_fd_read, "(iiii)i"},
    {"fd_close", wasi_fd_close, "(i)i"},
    {"fd_seek", wasi_fd_seek, "(iiIi)i"},
    {"path_open", wasi_path_open, "(iiiiiiiIIiI)i"},
    {"proc_exit", wasi_proc_exit, "(i)"},
    {"clock_time_get", wasi_clock_time_get, "(iIi)i"},
    /* Add more as needed */
};

#define NUM_WASI_SYMBOLS (sizeof(wasi_native_symbols) / sizeof(NativeSymbol))

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int ixland_wamr_init(void) {
    /* Initialize WASI fd table */
    memset(wasi_fd_table, 0xff, sizeof(wasi_fd_table));

    /* Preopen stdin, stdout, stderr */
    wasi_fd_table[0].real_fd = STDIN_FILENO;
    wasi_fd_table[0].wasi_fd = 0;
    wasi_fd_table[0].is_preopened = true;
    strcpy(wasi_fd_table[0].path, "/dev/stdin");

    wasi_fd_table[1].real_fd = STDOUT_FILENO;
    wasi_fd_table[1].wasi_fd = 1;
    wasi_fd_table[1].is_preopened = true;
    strcpy(wasi_fd_table[1].path, "/dev/stdout");

    wasi_fd_table[2].real_fd = STDERR_FILENO;
    wasi_fd_table[2].wasi_fd = 2;
    wasi_fd_table[2].is_preopened = true;
    strcpy(wasi_fd_table[2].path, "/dev/stderr");

    return 0;
}

void ixland_wamr_deinit(void) {
    /* Close all WASI fds */
    for (int i = 3; i < WASI_MAX_FD; i++) {
        if (wasi_fd_table[i].real_fd >= 0) {
            close(wasi_fd_table[i].real_fd);
            wasi_fd_table[i].real_fd = -1;
        }
    }
}

/* ============================================================================
 * RUNTIME OPERATIONS
 * ============================================================================ */

int ixland_wamr_load_wasm(const char *wasm_file, int argc, char **argv) {
    /* Read WASM file */
    FILE *file = fopen(wasm_file, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", wasm_file, strerror(errno));
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *wasm_buffer = malloc(file_size);
    if (!wasm_buffer) {
        fclose(file);
        fprintf(stderr, "Error: Out of memory\n");
        return -1;
    }

    if (fread(wasm_buffer, 1, file_size, file) != (size_t)file_size) {
        fclose(file);
        free(wasm_buffer);
        fprintf(stderr, "Error: Failed to read %s\n", wasm_file);
        return -1;
    }
    fclose(file);

    /* Initialize WAMR runtime */
    if (!wasm_runtime_init()) {
        free(wasm_buffer);
        fprintf(stderr, "Error: Failed to initialize WAMR runtime\n");
        return -1;
    }

    /* Register WASI native symbols */
    if (!wasm_runtime_register_natives("wasi_snapshot_preview1", wasi_native_symbols,
                                       NUM_WASI_SYMBOLS)) {
        wasm_runtime_destroy();
        free(wasm_buffer);
        fprintf(stderr, "Error: Failed to register WASI symbols\n");
        return -1;
    }

    /* Load WASM module */
    char error_buf[128] = {0};
    wasm_module_t module = wasm_runtime_load(wasm_buffer, file_size, error_buf, sizeof(error_buf));
    free(wasm_buffer);

    if (!module) {
        wasm_runtime_destroy();
        fprintf(stderr, "Error: Failed to load WASM module: %s\n", error_buf);
        return -1;
    }

    /* Instantiate module */
    uint32_t stack_size = 64 * 1024;
    uint32_t heap_size = 64 * 1024;

    wasm_module_inst_t module_inst =
        wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, sizeof(error_buf));
    if (!module_inst) {
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        fprintf(stderr, "Error: Failed to instantiate WASM module: %s\n", error_buf);
        return -1;
    }

    /* Create execution environment */
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
    if (!exec_env) {
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        fprintf(stderr, "Error: Failed to create execution environment\n");
        return -1;
    }

    /* Set up WASI args */
    wasm_runtime_set_wasi_args(module_inst, NULL, 0, NULL, 0, (const char **)argv, argc, NULL, 0);

    /* Find and call _start function */
    wasm_function_inst_t start_func = wasm_runtime_lookup_wasi_start_function(module_inst);
    if (!start_func) {
        fprintf(stderr, "Error: _start function not found\n");
        wasm_runtime_destroy_exec_env(exec_env);
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return -1;
    }

    /* Execute */
    uint32_t retval = 0;
    bool success = wasm_runtime_call_wasm(exec_env, start_func, 0, NULL, &retval);

    /* Cleanup */
    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    if (!success) {
        fprintf(stderr, "Error: WASM execution failed\n");
        return -1;
    }

    return (int)retval;
}

/* ============================================================================
 * COMMAND LINE TOOL
 * ============================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wasm-file> [args...]\n", argv[0]);
        fprintf(stderr, "\nRun a WebAssembly binary with WASI support\n");
        return 1;
    }

    const char *wasm_file = argv[1];

    /* Initialize libixland */
    ixland_init();
    ixland_wamr_init();

    /* Run WASM */
    int ret = ixland_wamr_load_wasm(wasm_file, argc - 1, argv + 1);

    ixland_wamr_deinit();

    return ret;
}
