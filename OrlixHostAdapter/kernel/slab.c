#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/compiler_types.h>
#include <linux/types.h>
#include <linux/gfp_types.h>

void *__kmalloc_noprof(size_t size, gfp_t flags) {
    size_t actual = size == 0 ? 1 : size;
    void *ptr = malloc(actual);

    if (ptr && (flags & __GFP_ZERO) != 0) {
        memset(ptr, 0, actual);
    }
    return ptr;
}

void kfree(const void *ptr) {
    free((void *)ptr);
}

char *kstrdup(const char *src, gfp_t flags) {
    size_t len;
    char *copy;

    if (!src) {
        return NULL;
    }

    len = strlen(src) + 1;
    copy = __kmalloc_noprof(len, flags);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, len);
    return copy;
}

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    int ret;

    ret = vsnprintf(buf, size, fmt, args);
    if (ret < 0) {
        return ret;
    }
    if (size == 0) {
        return 0;
    }
    if ((size_t)ret >= size) {
        return (int)(size - 1);
    }
    return ret;
}

int scnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vscnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}
