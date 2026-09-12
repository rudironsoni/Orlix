// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>

int main(void)
{
	char buf[16];

	if (sprintf(buf, "%0#6.3x", 1) < 0)
		return 2;
	if (strcmp(buf, " 0x001") != 0)
		return 3;
	if (sprintf(buf, "%0#6.3x", 0) < 0)
		return 4;
	if (strcmp(buf, "   000") != 0)
		return 5;
	return 0;
}
