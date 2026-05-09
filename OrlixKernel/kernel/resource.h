#ifndef KERNEL_RESOURCE_H
#define KERNEL_RESOURCE_H

#include <linux/resource.h>
#include <linux/times.h>

long times_impl(struct tms *buf);
int getrusage_impl(int who, struct rusage *usage);

#endif
