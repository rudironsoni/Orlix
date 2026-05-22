// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>
#include <linux/console.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <internal/asm/host_console.h>

static void orlix_tty_console_write(struct console *console,
				    const char *bytes,
				    unsigned int length)
{
	(void)console;
	orlix_host_console_write(bytes, length);
}

static int __init orlix_tty_console_setup(struct console *console,
					  char *options)
{
	(void)options;
	if (console->index < 0)
		console->index = 0;
	return console->index == 0 ? 0 : -ENODEV;
}

static struct console orlix_tty_console = {
	.name = "ttyS",
	.write = orlix_tty_console_write,
	.setup = orlix_tty_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};

static int __init orlix_tty_console_init(void)
{
	register_console(&orlix_tty_console);
	return 0;
}

console_initcall(orlix_tty_console_init);

MODULE_LICENSE("GPL");
