// SPDX-License-Identifier: GPL-2.0
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool file_is_readable(const char *path)
{
	char buffer[256];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0;
}

static bool proc_net_files_are_readable(void)
{
	return file_is_readable("/proc/net/dev") &&
	       file_is_readable("/proc/net/tcp") &&
	       file_is_readable("/proc/net/udp");
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
	socklen_t address_len = sizeof(address);
	char byte = 'x';
	char received = '\0';
	int listener = -1;
	int client = -1;
	int accepted = -1;
	bool ok = false;

	listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (listener < 0)
		goto out;

	address.sin_family = AF_INET;
	address.sin_port = 0;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0)
		goto out;

	if (getsockname(listener, (struct sockaddr *)&address, &address_len) < 0)
		goto out;

	if (listen(listener, 1) < 0)
		goto out;

	client = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (client < 0)
		goto out;

	if (connect(client, (struct sockaddr *)&address, sizeof(address)) < 0)
		goto out;

	accepted = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	if (accepted < 0)
		goto out;

	if (write(client, &byte, 1) != 1)
		goto out;
	if (read(accepted, &received, 1) != 1)
		goto out;

	ok = received == byte;

out:
	if (accepted >= 0)
		close(accepted);
	if (client >= 0)
		close(client);
	if (listener >= 0)
		close(listener);
	return ok;
}

static bool loopback_udp_exchanges_datagram(void)
{
	struct sockaddr_in address;
	socklen_t address_len = sizeof(address);
	char byte = 'u';
	char received = '\0';
	int server = -1;
	int client = -1;
	bool ok = false;

	server = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (server < 0)
		goto out;

	address.sin_family = AF_INET;
	address.sin_port = 0;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0)
		goto out;

	if (getsockname(server, (struct sockaddr *)&address, &address_len) < 0)
		goto out;

	client = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (client < 0)
		goto out;

	if (sendto(client, &byte, 1, 0, (struct sockaddr *)&address,
		   sizeof(address)) != 1)
		goto out;
	if (recvfrom(server, &received, 1, 0, NULL, NULL) != 1)
		goto out;

	ok = received == byte;

out:
	if (client >= 0)
		close(client);
	if (server >= 0)
		close(server);
	return ok;
}

int main(void)
{
	orlix_test_plan(4);

	orlix_test_result(proc_net_files_are_readable(),
			  "procfs exposes network state");
	orlix_test_result(rtnetlink_socket_opens(),
			  "rtnetlink sockets open in the current network namespace");
	orlix_test_result(loopback_tcp_accepts_connection(),
			  "loopback TCP accepts local connections");
	orlix_test_result(loopback_udp_exchanges_datagram(),
			  "loopback UDP exchanges local datagrams");

	orlix_test_exit();
}
