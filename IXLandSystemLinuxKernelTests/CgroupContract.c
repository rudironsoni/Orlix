#include "CgroupContract.h"

#include <linux/fcntl.h>

#include <errno.h>
#include <string.h>

#include "kernel/cgroup.h"
#include "kernel/task.h"

extern int open_impl(const char *pathname, int flags, unsigned int mode);
extern long read_impl(int fd, void *buf, unsigned long count);
extern int close_impl(int fd);

int cgroup_contract_current_task_starts_in_root(void) {
    struct task_struct *task = get_current();

    if (!task) {
        errno = ESRCH;
        return -1;
    }
    if (strcmp(task_cgroup_path(task), "/") != 0) {
        errno = ENODATA;
        return -1;
    }
    if (task_cgroup_member_count(task) == 0) {
        errno = ENOMSG;
        return -1;
    }
    return 0;
}

int cgroup_contract_child_inherits_parent_cgroup(void) {
    struct task_struct *parent = get_current();
    struct task_struct *child;
    unsigned int before;
    int ret = -1;

    if (!parent) {
        errno = ESRCH;
        return -1;
    }

    before = task_cgroup_member_count(parent);
    child = task_create_child_impl(parent);
    if (!child) {
        return -1;
    }
    if (strcmp(task_cgroup_path(child), task_cgroup_path(parent)) != 0) {
        errno = ENODATA;
        goto out;
    }
    if (task_cgroup_member_count(parent) != before + 1) {
        errno = ENOMSG;
        goto out;
    }

    ret = 0;

out:
    {
        int saved_errno = errno;
        task_unlink_child_impl(parent, child);
        free_task(child);
        if (task_cgroup_member_count(parent) != before) {
            ret = -1;
            saved_errno = EBUSY;
        }
        errno = saved_errno;
    }
    return ret;
}

int cgroup_contract_proc_self_cgroup_reports_root(void) {
    char buf[32];
    int fd;
    long nread;

    fd = open_impl("/proc/self/cgroup", O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    nread = read_impl(fd, buf, sizeof(buf) - 1);
    {
        int saved_errno = errno;
        close_impl(fd);
        errno = saved_errno;
    }
    if (nread != 5 || strcmp(buf, "0::/\n") != 0) {
        errno = ENODATA;
        return -1;
    }
    return 0;
}
