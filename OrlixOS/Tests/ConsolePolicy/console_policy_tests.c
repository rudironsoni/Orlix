#include "../../Sources/init/console_policy.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_policy(const char *command_line, int expected_result,
			  const char *expected_path)
{
	enum orlix_console_device device = ORLIX_CONSOLE_DEVICE_HVC0;
	int result = orlix_console_policy_resolve(command_line, &device);
	const char *path = result == ORLIX_CONSOLE_POLICY_OK ?
		orlix_console_device_path(device) : NULL;

	if (result == expected_result &&
	    ((path == NULL && expected_path == NULL) ||
	     (path != NULL && expected_path != NULL &&
	      strcmp(path, expected_path) == 0)))
		return;

	fprintf(stderr, "console policy mismatch for '%s': result=%d path=%s\n",
		command_line, result, path == NULL ? "(null)" : path);
	failures++;
}

int main(void)
{
	expect_policy("console=ttyS0 console=hvc0", ORLIX_CONSOLE_POLICY_OK,
		      "/dev/hvc0");
	expect_policy("console=hvc0 console=ttyS0", ORLIX_CONSOLE_POLICY_OK,
		      "/dev/ttyS0");
	expect_policy("console=ttyS0,115200n8", ORLIX_CONSOLE_POLICY_OK,
		      "/dev/ttyS0");
	expect_policy("console=hvc0", ORLIX_CONSOLE_POLICY_OK, "/dev/hvc0");
	expect_policy("rdinit=/init", ORLIX_CONSOLE_POLICY_MISSING, NULL);
	expect_policy("console=tty0", ORLIX_CONSOLE_POLICY_UNSUPPORTED, NULL);
	expect_policy("console=hvc0 console=tty0",
		      ORLIX_CONSOLE_POLICY_UNSUPPORTED, NULL);
	expect_policy("console=hvc0 console=hvc0", ORLIX_CONSOLE_POLICY_OK,
		      "/dev/hvc0");
	expect_policy("console=hvc0,9600n8", ORLIX_CONSOLE_POLICY_OK,
		      "/dev/hvc0");
	expect_policy("console=", ORLIX_CONSOLE_POLICY_EMPTY, NULL);
	expect_policy("console=ttyS0 console=", ORLIX_CONSOLE_POLICY_EMPTY, NULL);

	if (failures != 0)
		return 1;
	puts("console policy tests: 11 passed");
	return 0;
}
