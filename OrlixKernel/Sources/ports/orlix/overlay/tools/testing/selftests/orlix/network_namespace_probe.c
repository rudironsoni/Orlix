// SPDX-License-Identifier: GPL-2.0

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool file_is_readable(const char *path)
{
	char buffer[256];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
	       size > 0;
}

static bool rtnetlink_socket_opens(void)
{
	int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

	if (fd < 0)
		return false;
	close(fd);
	return true;
}

static bool loopback_tcp_accepts_connection(void)
{
	struct sockaddr_in address;
	socklen_t address_length = sizeof(address);
	char byte = 'n';
	int listener = -1;
	int client = -1;
	int accepted = -1;
	bool result = false;

	listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (listener < 0)
		goto out;

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = 0;

	if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0)
		goto out;
	if (listen(listener, 1) != 0)
		goto out;
	if (getsockname(listener, (struct sockaddr *)&address,
			&address_length) != 0)
		goto out;

	client = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (client < 0)
		goto out;
	if (connect(client, (struct sockaddr *)&address, sizeof(address)) != 0)
		goto out;

	accepted = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	if (accepted < 0)
		goto out;

	result = write(client, &byte, sizeof(byte)) == sizeof(byte) &&
		 read(accepted, &byte, sizeof(byte)) == sizeof(byte) &&
		 byte == 'n';

out:
	if (accepted >= 0)
		close(accepted);
	if (client >= 0)
		close(client);
	if (listener >= 0)
		close(listener);
	return result;
}

int main(void)
{
	orlix_test_plan(4);

	orlix_test_result(file_is_readable("/proc/net/dev"),
			  "procfs exposes network device state");
	orlix_test_result(file_is_readable("/proc/net/tcp"),
			  "procfs exposes TCP socket state");
	orlix_test_result(rtnetlink_socket_opens(),
			  "rtnetlink sockets open in the current network namespace");
	orlix_test_result(loopback_tcp_accepts_connection(),
			  "loopback TCP accepts local connections");

	orlix_test_exit();
}
