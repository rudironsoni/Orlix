#ifndef IXLAND_INTERNAL_H
#define IXLAND_INTERNAL_H

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

/* Include IXLand types for dirent structure */
#include "../include/ixland/ixland_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Include path types from owner header */
#include "../include/ixland/ixland_path.h"

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define IXLAND_MAX_FD 256
#define IXLAND_MAX_PATH 4096

/* Directory entry types */
#define IXLAND_DT_UNKNOWN 0
#define IXLAND_DT_FIFO 1
#define IXLAND_DT_CHR 2
#define IXLAND_DT_DIR 4
#define IXLAND_DT_BLK 6
#define IXLAND_DT_REG 8
#define IXLAND_DT_LNK 10
#define IXLAND_DT_SOCK 12
#define IXLAND_DT_WHT 14

/* ============================================================================
 * FD Table Implementation
 * ============================================================================ */

/**
 * Allocate a new FD from the global pool.
 * @return allocated FD or -1 on error
 */
int __ixland_alloc_fd_impl(void);

/**
 * Free an FD back to the global pool.
 * @param fd The FD to free
 */
void __ixland_free_fd_impl(int fd);

/**
 * Initialize a file entry with the given parameters.
 * @param fd The FD to initialize
 * @param real_fd The real host FD
 * @param flags Open flags
 * @param mode File mode
 * @param path File path
 */
void __ixland_init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path);

/**
 * Get an FD entry by FD number.
 * @param fd The FD to look up
 * @return pointer to entry or NULL
 */
void *__ixland_get_fd_entry_impl(int fd);

/**
 * Release a reference to an FD entry.
 * @param entry The entry to release
 */
void __ixland_put_fd_entry_impl(void *entry);

/**
 * Get the real host FD from an FD entry.
 * @param entry The FD entry
 * @return the real host FD
 */
int __ixland_get_real_fd_impl(void *entry);

/**
 * Get the flags from an FD entry.
 * @param entry The FD entry
 * @return the flags
 */
int __ixland_get_fd_flags_impl(void *entry);

/**
 * Set the flags on an FD entry.
 * @param entry The FD entry
 * @param flags The flags to set
 */
void __ixland_set_fd_flags_impl(void *entry, int flags);

/**
 * Get the current offset from an FD entry.
 * @param entry The FD entry
 * @return the current offset
 */
off_t __ixland_get_fd_offset_impl(void *entry);

/**
 * Set the current offset on an FD entry.
 * @param entry The FD entry
 * @param offset The offset to set
 */
void __ixland_set_fd_offset_impl(void *entry, off_t offset);

/**
 * Clone an FD entry.
 * @param newfd The new FD number
 * @param oldfd The old FD number
 */
void __ixland_clone_fd_entry_impl(int newfd, int oldfd);

/**
 * Close an FD.
 * @param fd The FD to close
 * @return 0 on success, -1 on error
 */
int __ixland_close_impl(int fd);

/* ============================================================================
 * Task and Process Implementation
 * ============================================================================ */

/**
 * Initialize the task subsystem.
 */
void __ixland_task_init_impl(void);

/**
 * Allocate a new task.
 * @return new task or NULL on error
 */
struct ixland_task *__ixland_task_alloc_impl(void);

/**
 * Free a task structure.
 * @param task The task to free
 */
void __ixland_task_free_impl(struct ixland_task *task);

/**
 * Add a task to the global task table.
 * @param task The task to add
 * @return 0 on success, -1 on error
 */
int __ixland_task_add_impl(struct ixland_task *task);

/**
 * Remove a task from the global task table.
 * @param task The task to remove
 */
void __ixland_task_remove_impl(struct ixland_task *task);

/**
 * Get a task by PID.
 * @param pid The PID to look up
 * @return The task or NULL if not found
 */
struct ixland_task *__ixland_task_lookup_impl(pid_t pid);

/**
 * Check if the current task is the init task.
 * @return true if this is the init task
 */
bool __ixland_is_init_task_impl(void);

/**
 * Get the current task.
 * @return the current task or NULL
 */
struct ixland_task *__ixland_current_task_impl(void);

/**
 * Set the current task.
 * @param task The task to set as current
 */
void __ixland_set_current_task_impl(struct ixland_task *task);

/**
 * Save the current task context.
 */
void __ixland_save_task_context_impl(void);

/**
 * Restore the task context.
 */
void __ixland_restore_task_context_impl(void);

/**
 * Validate a task pointer.
 * @param task The task to validate
 * @return true if valid
 */
bool __ixland_task_is_valid_impl(struct ixland_task *task);

/**
 * Get the next task in the global list.
 * @param task The current task
 * @return the next task or NULL
 */
struct ixland_task *__ixland_task_next_impl(struct ixland_task *task);

/**
 * Initialize a new task from a parent.
 * @param task The task to initialize
 * @param parent The parent task
 * @return 0 on success, -1 on error
 */
int __ixland_task_init_from_parent_impl(struct ixland_task *task, struct ixland_task *parent);

/**
 * Complete task setup after initialization.
 * @param task The task to complete
 * @return 0 on success, -1 on error
 */
int __ixland_task_setup_complete_impl(struct ixland_task *task);

/**
 * Update task state.
 * @param task The task to update
 * @param state The new state
 */
void __ixland_task_update_state_impl(struct ixland_task *task, int state);

/**
 * Set task exit status.
 * @param task The task
 * @param status The exit status
 */
void __ixland_task_set_exit_status_impl(struct ixland_task *task, int status);

/**
 * Get task exit status.
 * @param task The task
 * @return the exit status
 */
int __ixland_task_get_exit_status_impl(struct ixland_task *task);

/**
 * Notify parent of child state change.
 * @param child The child task
 * @param notify The parent to notify
 */
void __ixland_task_notify_parent_impl(struct ixland_task *child, struct ixland_task *notify);

/**
 * Wait for task exit.
 * @param pid The PID to wait for
 * @param status Output for exit status
 * @param options Wait options
 * @return the PID or -1 on error
 */
pid_t __ixland_task_wait_impl(pid_t pid, int *status, int options);

/**
 * Enter critical section.
 */
void __ixland_enter_critical_impl(void);

/**
 * Exit critical section.
 */
void __ixland_exit_critical_impl(void);

/* ============================================================================
 * Signal Implementation
 * ============================================================================ */

/**
 * Initialize signal handling for a task.
 * @param task The task
 * @return 0 on success, -1 on error
 */
int __ixland_signal_init_impl(struct ixland_task *task);

/**
 * Cleanup signal handling for a task.
 * @param task The task
 */
void __ixland_signal_cleanup_impl(struct ixland_task *task);

/**
 * Check for pending signals.
 * @return true if signals are pending
 */
bool __ixland_signal_pending_impl(void);

/**
 * Deliver pending signals.
 */
void __ixland_signal_deliver_pending_impl(void);

/**
 * Block a signal.
 * @param sig The signal number
 * @return 0 on success, -1 on error
 */
int __ixland_signal_block_impl(int sig);

/**
 * Unblock a signal.
 * @param sig The signal number
 * @return 0 on success, -1 on error
 */
int __ixland_signal_unblock_impl(int sig);

/**
 * Send a signal to a task.
 * @param task The target task
 * @param sig The signal number
 * @return 0 on success, -1 on error
 */
int __ixland_signal_send_impl(struct ixland_task *task, int sig);

/**
 * Handle a signal.
 * @param sig The signal number
 */
void __ixland_signal_handle_impl(int sig);

/* ============================================================================
 * Filesystem Implementation
 * ============================================================================ */

/**
 * Initialize the filesystem subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_fs_init_impl(void);

/**
 * Get the current working directory.
 * @param buf Buffer to store path
 * @param size Buffer size
 * @return pointer to buf on success, NULL on error
 */
char *__ixland_getcwd_impl(char *buf, size_t size);

/**
 * Initialize path utilities.
 * @return 0 on success, -1 on error
 */
int __ixland_path_init_impl(void);

/**
 * Cleanup path utilities.
 */
void __ixland_path_cleanup_impl(void);

/**
 * Classify a path type.
 * @param path The path to classify
 * @return the path type
 */
ixland_path_type_t __ixland_path_classify(const char *path);

/**
 * Normalize a path in place.
 * @param path The path to normalize
 */
void __ixland_path_normalize(char *path);

/**
 * Resolve a path to an absolute path.
 * @param path The path to resolve
 * @param resolved Buffer for resolved path
 * @param resolved_len Buffer length
 * @return 0 on success, -1 on error
 */
int __ixland_path_resolve(const char *path, char *resolved, size_t resolved_len);

/**
 * Join two paths.
 * @param base The base path
 * @param rel The relative path
 * @param result Buffer for result
 * @param result_len Buffer length
 */
void __ixland_path_join(const char *base, const char *rel, char *result, size_t result_len);

/**
 * Check if path is in sandbox.
 * @param path The path to check
 * @return true if in sandbox
 */
bool __ixland_path_in_sandbox(const char *path);

/**
 * Check if path is a virtual Linux path.
 * @param path The path to check
 * @return true if virtual Linux path
 */
bool __ixland_path_is_virtual_linux(const char *path);

/**
 * Check if path is in own sandbox.
 * @param path The path to check
 * @return true if in own sandbox
 */
bool __ixland_path_is_own_sandbox(const char *path);

/**
 * Check if path is external.
 * @param path The path to check
 * @return true if external
 */
bool __ixland_path_is_external(const char *path);

/**
 * Convert virtual path to iOS path.
 * @param vpath Virtual path
 * @param ios_path iOS path buffer
 * @param ios_path_len Buffer size
 * @return 0 on success, -1 on error
 */
int __ixland_path_virtual_to_ios(const char *vpath, char *ios_path, size_t ios_path_len);

/**
 * Check if path is a direct path (not virtual).
 * @param path The path to check
 * @return true if direct
 */
bool __ixland_path_is_direct(const char *path);

/**
 * Initialize mount subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_mount_init_impl(void);

/**
 * Cleanup mount subsystem.
 */
void __ixland_mount_cleanup_impl(void);

/**
 * Initialize superblock subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_super_init_impl(void);

/**
 * Cleanup superblock subsystem.
 */
void __ixland_super_cleanup_impl(void);

/**
 * Initialize inode subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_inode_init_impl(void);

/**
 * Cleanup inode subsystem.
 */
void __ixland_inode_cleanup_impl(void);

/**
 * Initialize namei (path lookup) subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_namei_init_impl(void);

/**
 * Cleanup namei subsystem.
 */
void __ixland_namei_cleanup_impl(void);

/* ============================================================================
 * Credentials Implementation
 * ============================================================================ */

/**
 * Initialize credentials subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_cred_init_impl(void);

/**
 * Cleanup credentials subsystem.
 */
void __ixland_cred_cleanup_impl(void);

/* ============================================================================
 * Time Implementation
 * ============================================================================ */

/**
 * Initialize time subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_time_init_impl(void);

/* ============================================================================
 * Random Implementation
 * ============================================================================ */

/**
 * Initialize random subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_random_init_impl(void);

/**
 * Get random bytes.
 * @param buf Buffer to fill
 * @param len Number of bytes
 * @return 0 on success, -1 on error
 */
int __ixland_getrandom_impl(void *buf, size_t len, unsigned int flags);

/* ============================================================================
 * Network Implementation
 * ============================================================================ */

/**
 * Initialize network subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_network_init_impl(void);

/**
 * Cleanup network subsystem.
 */
void __ixland_network_cleanup_impl(void);

/* ============================================================================
 * Synchronization Implementation
 * ============================================================================ */

/**
 * Initialize sync primitives subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_sync_init_impl(void);

/**
 * Cleanup sync primitives subsystem.
 */
void __ixland_sync_cleanup_impl(void);

/* ============================================================================
 * Resource Limits Implementation
 * ============================================================================ */

/**
 * Initialize resource limits subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_rlimit_init_impl(void);

/**
 * Get resource limit.
 * @param resource The resource
 * @param rlim The limit structure
 * @return 0 on success, -1 on error
 */
int __ixland_getrlimit_impl(int resource, struct rlimit *rlim);

/**
 * Set resource limit.
 * @param resource The resource
 * @param rlim The limit structure
 * @return 0 on success, -1 on error
 */
int __ixland_setrlimit_impl(int resource, const struct rlimit *rlim);

/* ============================================================================
 * TTY Implementation
 * ============================================================================ */

/**
 * Initialize TTY subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_tty_init_impl(void);

/**
 * Cleanup TTY subsystem.
 */
void __ixland_tty_cleanup_impl(void);

/**
 * Allocate a TTY.
 * @return new TTY or NULL on error
 */
struct ixland_tty *__ixland_tty_alloc_impl(void);

/**
 * Free a TTY.
 * @param tty The TTY to free
 */
void __ixland_tty_free_impl(struct ixland_tty *tty);

/**
 * Open a TTY device.
 * @param tty The TTY
 * @return 0 on success, -1 on error
 */
int __ixland_tty_open_impl(struct ixland_tty *tty);

/**
 * Close a TTY device.
 * @param tty The TTY
 */
void __ixland_tty_close_impl(struct ixland_tty *tty);

/**
 * Set TTY as controlling.
 * @param task The task
 * @param tty The TTY
 * @return 0 on success, -1 on error
 */
int __ixland_set_controlling_tty_impl(struct ixland_task *task, struct ixland_tty *tty);

/**
 * Release controlling TTY.
 * @param task The task
 * @return 0 on success, -1 on error
 */
int __ixland_release_controlling_tty_impl(struct ixland_task *task);

/* ============================================================================
 * Process Group and Session Implementation
 * ============================================================================ */

/**
 * Initialize process group subsystem.
 * @return 0 on success, -1 on error
 */
int __ixland_pgrp_init_impl(void);

/**
 * Cleanup process group subsystem.
 */
void __ixland_pgrp_cleanup_impl(void);

/**
 * Join a process group.
 * @param task The task
 * @param pgid The process group ID
 * @return 0 on success, -1 on error
 */
int __ixland_join_pgrp_impl(struct ixland_task *task, pid_t pgid);

/**
 * Leave current process group.
 * @param task The task
 * @return 0 on success, -1 on error
 */
int __ixland_leave_pgrp_impl(struct ixland_task *task);

/**
 * Create a new session.
 * @param task The task
 * @return new session ID or -1 on error
 */
pid_t __ixland_create_session_impl(struct ixland_task *task);

/**
 * Join a session.
 * @param task The task
 * @param sid The session ID
 * @return 0 on success, -1 on error
 */
int __ixland_join_session_impl(struct ixland_task *task, pid_t sid);

/**
 * Get session leader.
 * @param sid The session ID
 * @return the session leader task or NULL
 */
struct ixland_task *__ixland_get_session_leader_impl(pid_t sid);

/* ============================================================================
 * Exec Implementation
 * ============================================================================ */

/**
 * Execute a native binary.
 * @param task The task
 * @param path The binary path
 * @param argv Argument vector
 * @param envp Environment vector
 * @return 0 on success, -1 on error
 */
int __ixland_exec_native_impl(struct ixland_task *task, const char *path, char *const argv[],
                              char *const envp[]);

/**
 * Execute a WASM binary.
 * @param task The task
 * @param path The binary path
 * @param argv Argument vector
 * @param envp Environment vector
 * @return 0 on success, -1 on error
 */
int __ixland_exec_wasm_impl(struct ixland_task *task, const char *path, char *const argv[],
                            char *const envp[]);

/**
 * Execute a script.
 * @param task The task
 * @param path The script path
 * @param argv Argument vector
 * @param envp Environment vector
 * @return 0 on success, -1 on error
 */
int __ixland_exec_script_impl(struct ixland_task *task, const char *path, char *const argv[],
                              char *const envp[]);

/**
 * Complete exec operation.
 * @param task The task
 */
void __ixland_exec_complete_impl(struct ixland_task *task);

/* ============================================================================
 * Registry Implementation
 * ============================================================================ */

/**
 * Initialize native command registry.
 * @return 0 on success, -1 on error
 */
int __ixland_registry_init_impl(void);

/**
 * Cleanup native command registry.
 */
void __ixland_registry_cleanup_impl(void);

/**
 * Lookup a native command.
 * @param name The command name
 * @return the command entry or NULL
 */
struct ixland_native_cmd *__ixland_registry_lookup_impl(const char *name);

/* ============================================================================
 * Syscall Table Implementation
 * ============================================================================ */

/**
 * Initialize syscall table.
 * @return 0 on success, -1 on error
 */
int __ixland_syscall_table_init_impl(void);

/**
 * Register a syscall handler.
 * @param nr The syscall number
 * @param handler The handler function
 * @return 0 on success, -1 on error
 */
int __ixland_syscall_register_impl(long nr, void *handler);

/**
 * Get syscall handler.
 * @param nr The syscall number
 * @return the handler or NULL
 */
void *__ixland_syscall_get_handler_impl(long nr);

/* ============================================================================
 * VFS Implementation
 * ============================================================================ */

/**
 * Initialize VFS layer.
 * @return 0 on success, -1 on error
 */
int __ixland_vfs_init_impl(void);

/**
 * Cleanup VFS layer.
 */
void __ixland_vfs_cleanup_impl(void);

/**
 * Open a file through VFS.
 * @param vpath Virtual path
 * @param flags Open flags
 * @param mode File mode
 * @param vnode Output vnode
 * @return 0 on success, -1 on error
 */
int __ixland_vfs_open_impl(const char *vpath, int flags, mode_t mode, void **vnode);

/**
 * Close a VFS file.
 * @param vnode The vnode
 */
void __ixland_vfs_close_impl(void *vnode);

/**
 * Read from VFS file.
 * @param vnode The vnode
 * @param buf Buffer
 * @param count Bytes to read
 * @return bytes read or -1 on error
 */
ssize_t __ixland_vfs_read_impl(void *vnode, void *buf, size_t count);

/**
 * Write to VFS file.
 * @param vnode The vnode
 * @param buf Buffer
 * @param count Bytes to write
 * @return bytes written or -1 on error
 */
ssize_t __ixland_vfs_write_impl(void *vnode, const void *buf, size_t count);

/**
 * Seek in VFS file.
 * @param vnode The vnode
 * @param offset Offset
 * @param whence Seek type
 * @return new position or -1 on error
 */
off_t __ixland_vfs_lseek_impl(void *vnode, off_t offset, int whence);

/**
 * Stat VFS file.
 * @param vnode The vnode
 * @param statbuf Stat buffer
 * @return 0 on success, -1 on error
 */
int __ixland_vfs_stat_impl(void *vnode, struct stat *statbuf);

/**
 * Translate virtual path to iOS path.
 * @param vpath Virtual path
 * @param ios_path iOS path buffer
 * @param ios_path_len Buffer size
 * @return 0 on success, -1 on error
 */
int __ixland_vfs_translate_impl(const char *vpath, char *ios_path, size_t ios_path_len);

/**
 * Reverse translate iOS path to virtual path.
 * @param ios_path iOS path
 * @param vpath Virtual path buffer
 * @param vpath_len Buffer size
 * @return 0 on success, -1 on error
 */
int __ixland_vfs_reverse_translate_impl(const char *ios_path, char *vpath, size_t vpath_len);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_INTERNAL_H */
