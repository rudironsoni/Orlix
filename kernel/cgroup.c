/* IXLandSystem/kernel/cgroup.c
 * Virtual cgroup hierarchy and membership ownership.
 */

#include "cgroup.h"

#include "task.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cgroup {
    char path[MAX_PATH];
    atomic_int refs;
    unsigned int members;
    kernel_mutex_t lock;
};

static struct cgroup *root_cgroup;
static kernel_mutex_t cgroup_lock = KERNEL_MUTEX_INITIALIZER;

int cgroup_init(void) {
    kernel_mutex_lock(&cgroup_lock);
    if (root_cgroup) {
        kernel_mutex_unlock(&cgroup_lock);
        return 0;
    }

    root_cgroup = calloc(1, sizeof(*root_cgroup));
    if (!root_cgroup) {
        kernel_mutex_unlock(&cgroup_lock);
        return -ENOMEM;
    }

    memcpy(root_cgroup->path, "/", 2);
    atomic_init(&root_cgroup->refs, 1);
    kernel_mutex_init(&root_cgroup->lock);
    kernel_mutex_unlock(&cgroup_lock);
    return 0;
}

void cgroup_deinit(void) {
    struct cgroup *root;

    kernel_mutex_lock(&cgroup_lock);
    root = root_cgroup;
    root_cgroup = NULL;
    kernel_mutex_unlock(&cgroup_lock);

    if (!root) {
        return;
    }
    kernel_mutex_destroy(&root->lock);
    free(root);
}

struct cgroup *cgroup_get(struct cgroup *cgrp) {
    if (!cgrp) {
        return NULL;
    }
    atomic_fetch_add(&cgrp->refs, 1);
    return cgrp;
}

void cgroup_put(struct cgroup *cgrp) {
    if (!cgrp) {
        return;
    }
    if (atomic_fetch_sub(&cgrp->refs, 1) > 1) {
        return;
    }
    kernel_mutex_destroy(&cgrp->lock);
    free(cgrp);
}

struct cgroup *cgroup_root(void) {
    if (!root_cgroup && cgroup_init() != 0) {
        return NULL;
    }
    return cgroup_get(root_cgroup);
}

int task_attach_cgroup(struct task_struct *task, struct cgroup *cgrp) {
    struct cgroup *old;

    if (!task || !cgrp) {
        return -EINVAL;
    }

    old = task->cgroup;
    if (old == cgrp) {
        return 0;
    }

    cgroup_get(cgrp);
    kernel_mutex_lock(&cgrp->lock);
    cgrp->members++;
    kernel_mutex_unlock(&cgrp->lock);

    task->cgroup = cgrp;

    if (old) {
        kernel_mutex_lock(&old->lock);
        if (old->members > 0) {
            old->members--;
        }
        kernel_mutex_unlock(&old->lock);
        cgroup_put(old);
    }

    return 0;
}

void task_detach_cgroup(struct task_struct *task) {
    struct cgroup *old;

    if (!task || !task->cgroup) {
        return;
    }

    old = task->cgroup;
    task->cgroup = NULL;
    kernel_mutex_lock(&old->lock);
    if (old->members > 0) {
        old->members--;
    }
    kernel_mutex_unlock(&old->lock);
    cgroup_put(old);
}

const char *task_cgroup_path(const struct task_struct *task) {
    if (!task || !task->cgroup) {
        return "/";
    }
    return task->cgroup->path;
}

unsigned int task_cgroup_member_count(const struct task_struct *task) {
    struct cgroup *cgrp;
    unsigned int count;

    if (!task || !task->cgroup) {
        return 0;
    }

    cgrp = task->cgroup;
    kernel_mutex_lock(&cgrp->lock);
    count = cgrp->members;
    kernel_mutex_unlock(&cgrp->lock);
    return count;
}

int task_cgroup_proc_content(const struct task_struct *task, char *buf, size_t buflen) {
    int ret;

    if (!buf || buflen == 0) {
        return -EINVAL;
    }
    if (!task) {
        return -ESRCH;
    }

    ret = snprintf(buf, buflen, "0::%s\n", task_cgroup_path(task));
    if (ret < 0) {
        return -EINVAL;
    }
    if ((size_t)ret >= buflen) {
        return (int)(buflen - 1);
    }
    return ret;
}

int cgroup_proc_task_content(int32_t pid, char *buf, size_t buflen) {
    struct task_struct *task;
    int ret;

    task = task_lookup(pid);
    if (!task) {
        return -ESRCH;
    }
    ret = task_cgroup_proc_content(task, buf, buflen);
    free_task(task);
    return ret;
}
