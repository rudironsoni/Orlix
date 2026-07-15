// SPDX-License-Identifier: GPL-2.0

#include "console_policy.h"

#include <stddef.h>
#include <string.h>

int orlix_console_policy_resolve(const char *command_line,
				 enum orlix_console_device *device)
{
	static const char prefix[] = "console=";
	const char *selected = NULL;
	size_t selected_length = 0;
	const char *cursor;

	if (command_line == NULL || device == NULL)
		return ORLIX_CONSOLE_POLICY_MISSING;

	cursor = command_line;
	while (*cursor != '\0') {
		const char *end;
		const char *option;
		size_t token_length;

		while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
		       *cursor == '\r' || *cursor == '\f' || *cursor == '\v')
			cursor++;
		if (*cursor == '\0')
			break;

		end = cursor;
		while (*end != '\0' && *end != ' ' && *end != '\t' &&
		       *end != '\n' && *end != '\r' && *end != '\f' &&
		       *end != '\v')
			end++;
		token_length = (size_t)(end - cursor);
		if (token_length >= sizeof(prefix) - 1 &&
		    memcmp(cursor, prefix, sizeof(prefix) - 1) == 0) {
			selected = cursor + sizeof(prefix) - 1;
			selected_length = token_length - (sizeof(prefix) - 1);
			option = memchr(selected, ',', selected_length);
			if (option != NULL)
				selected_length = (size_t)(option - selected);
		}
		cursor = end;
	}

	if (selected == NULL)
		return ORLIX_CONSOLE_POLICY_MISSING;
	if (selected_length == 0)
		return ORLIX_CONSOLE_POLICY_EMPTY;
	if (selected_length == 4 && memcmp(selected, "hvc0", 4) == 0) {
		*device = ORLIX_CONSOLE_DEVICE_HVC0;
		return ORLIX_CONSOLE_POLICY_OK;
	}
	if (selected_length == 5 && memcmp(selected, "ttyS0", 5) == 0) {
		*device = ORLIX_CONSOLE_DEVICE_TTYS0;
		return ORLIX_CONSOLE_POLICY_OK;
	}

	return ORLIX_CONSOLE_POLICY_UNSUPPORTED;
}

const char *orlix_console_device_path(enum orlix_console_device device)
{
	switch (device) {
	case ORLIX_CONSOLE_DEVICE_HVC0:
		return "/dev/hvc0";
	case ORLIX_CONSOLE_DEVICE_TTYS0:
		return "/dev/ttyS0";
	}

	return NULL;
}
