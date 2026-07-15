// SPDX-License-Identifier: GPL-2.0

#ifndef ORLIX_CONSOLE_POLICY_H
#define ORLIX_CONSOLE_POLICY_H

enum orlix_console_policy_result {
	ORLIX_CONSOLE_POLICY_OK = 0,
	ORLIX_CONSOLE_POLICY_MISSING = -1,
	ORLIX_CONSOLE_POLICY_EMPTY = -2,
	ORLIX_CONSOLE_POLICY_UNSUPPORTED = -3,
};

enum orlix_console_device {
	ORLIX_CONSOLE_DEVICE_HVC0,
	ORLIX_CONSOLE_DEVICE_TTYS0,
};

int orlix_console_policy_resolve(const char *command_line,
				 enum orlix_console_device *device);
const char *orlix_console_device_path(enum orlix_console_device device);

#endif
