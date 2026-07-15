/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ORLIX_INTERNAL_ASM_HOST_CONSOLE_H
#define _ORLIX_INTERNAL_ASM_HOST_CONSOLE_H

enum orlix_host_console_source {
	ORLIX_HOST_CONSOLE_SOURCE_SERIAL = 0,
	ORLIX_HOST_CONSOLE_SOURCE_VIRTIO = 1,
};

void orlix_host_console_write(enum orlix_host_console_source source,
			      const void *bytes, unsigned long length);
unsigned long orlix_host_console_read_input(enum orlix_host_console_source source,
					    void *bytes, unsigned long length);

#endif
