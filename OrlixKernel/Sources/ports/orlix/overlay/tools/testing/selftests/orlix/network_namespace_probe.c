// SPDX-License-Identifier: GPL-2.0
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
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

static bool rtnetlink_reports_loopback_link(void)
{
	struct {
		struct nlmsghdr header;
		struct ifinfomsg interface;
	} request = {
		.header = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
			.nlmsg_seq = 1,
		},
		.interface = {
			.ifi_family = AF_UNSPEC,
		},
	};
	char buffer[8192];
	bool saw_loopback = false;
	int fd;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0)
		return false;

	if (send(fd, &request, request.header.nlmsg_len, 0) < 0) {
		close(fd);
		return false;
	}

	for (;;) {
		ssize_t received;
		struct nlmsghdr *message;

		received = recv(fd, buffer, sizeof(buffer), 0);
		if (received < 0) {
			close(fd);
			return false;
		}

		for (message = (struct nlmsghdr *)buffer;
		     NLMSG_OK(message, received);
		     message = NLMSG_NEXT(message, received)) {
			struct ifinfomsg *interface;
			struct rtattr *attribute;
			int attributes_len;

			if (message->nlmsg_type == NLMSG_DONE) {
				close(fd);
				return saw_loopback;
			}

			if (message->nlmsg_type == NLMSG_ERROR) {
				close(fd);
				return false;
			}

			if (message->nlmsg_type != RTM_NEWLINK)
				continue;

			interface = NLMSG_DATA(message);
			attributes_len = IFLA_PAYLOAD(message);

			for (attribute = IFLA_RTA(interface);
			     RTA_OK(attribute, attributes_len);
			     attribute = RTA_NEXT(attribute, attributes_len)) {
				const char *name;

				if (attribute->rta_type != IFLA_IFNAME)
					continue;

				name = RTA_DATA(attribute);
				if (interface->ifi_index > 0 &&
				    strcmp(name, "lo") == 0)
					saw_loopback = true;
			}
		}
	}
}

static bool configure_loopback_interface(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	struct ifreq ifr;
	struct sockaddr_in *addr;

	if (fd < 0)
		return false;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (ioctl(fd, SIOCSIFADDR, &ifr) < 0)
		goto fail;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htonl(0xff000000UL);
	if (ioctl(fd, SIOCSIFNETMASK, &ifr) < 0)
		goto fail;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
		goto fail;
	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0)
		goto fail;

	close(fd);
	return true;

fail:
	close(fd);
	return false;
}

static const char *loopback_tcp_failure_step = "none";

static bool rtnetlink_reports_loopback_ipv4_address(void)
{
	struct {
		struct nlmsghdr header;
		struct ifaddrmsg address;
	} request = {
		.header = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg)),
			.nlmsg_type = RTM_GETADDR,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
			.nlmsg_seq = 2,
		},
		.address = {
			.ifa_family = AF_INET,
		},
	};
	char buffer[8192];
	bool saw_loopback_address = false;
	int fd;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0)
		return false;

	if (send(fd, &request, request.header.nlmsg_len, 0) < 0) {
		close(fd);
		return false;
	}

	for (;;) {
		ssize_t received;
		struct nlmsghdr *message;

		received = recv(fd, buffer, sizeof(buffer), 0);
		if (received < 0) {
			close(fd);
			return false;
		}

		for (message = (struct nlmsghdr *)buffer;
		     NLMSG_OK(message, received);
		     message = NLMSG_NEXT(message, received)) {
			struct ifaddrmsg *address;
			struct rtattr *attribute;
			int attributes_len;

			if (message->nlmsg_type == NLMSG_DONE) {
				close(fd);
				return saw_loopback_address;
			}

			if (message->nlmsg_type == NLMSG_ERROR) {
				close(fd);
				return false;
			}

			if (message->nlmsg_type != RTM_NEWADDR)
				continue;

			address = NLMSG_DATA(message);
			if (address->ifa_family != AF_INET ||
			    address->ifa_prefixlen != 8 ||
			    address->ifa_index <= 0)
				continue;

			attributes_len = IFA_PAYLOAD(message);
			for (attribute = IFA_RTA(address);
			     RTA_OK(attribute, attributes_len);
			     attribute = RTA_NEXT(attribute, attributes_len)) {
				struct in_addr *ipv4;

				if (attribute->rta_type != IFA_ADDRESS &&
				    attribute->rta_type != IFA_LOCAL)
					continue;

				if (RTA_PAYLOAD(attribute) < sizeof(*ipv4))
					continue;

				ipv4 = RTA_DATA(attribute);
				if (ipv4->s_addr == htonl(INADDR_LOOPBACK))
					saw_loopback_address = true;
			}
		}
	}
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
	const char *failed_step = "none";
	bool ok = false;

	listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (listener < 0) {
		failed_step = "socket(listener)";
		goto out;
	}

	address.sin_family = AF_INET;
	address.sin_port = 0;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0) {
		failed_step = "bind";
		goto out;
	}

	if (getsockname(listener, (struct sockaddr *)&address, &address_len) < 0) {
		failed_step = "getsockname";
		goto out;
	}

	if (listen(listener, 1) < 0) {
		failed_step = "listen";
		goto out;
	}

	client = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (client < 0) {
		failed_step = "socket(client)";
		goto out;
	}

	if (connect(client, (struct sockaddr *)&address, sizeof(address)) < 0) {
		failed_step = "connect";
		goto out;
	}

	accepted = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	if (accepted < 0) {
		failed_step = "accept4";
		goto out;
	}

	if (write(client, &byte, 1) != 1) {
		failed_step = "write";
		goto out;
	}
	if (read(accepted, &received, 1) != 1) {
		failed_step = "read";
		goto out;
	}

	ok = received == byte;

out:
	if (accepted >= 0)
		close(accepted);
	if (client >= 0)
		close(client);
	if (listener >= 0)
		close(listener);
	if (!ok)
		loopback_tcp_failure_step = failed_step;
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
	orlix_test_plan(7);

	orlix_test_result(proc_net_files_are_readable(),
			  "procfs exposes network state");
	orlix_test_result(rtnetlink_socket_opens(),
			  "rtnetlink sockets open in the current network namespace");
	orlix_test_result(rtnetlink_reports_loopback_link(),
			  "RTM_GETLINK reports loopback interface");
	orlix_test_result(configure_loopback_interface(),
			  "loopback interface accepts Linux address configuration");
	orlix_test_result(rtnetlink_reports_loopback_ipv4_address(),
			  "RTM_GETADDR reports loopback IPv4 address");
	{
		bool tcp_ok = loopback_tcp_accepts_connection();

		orlix_test_result(tcp_ok,
				  tcp_ok ? "loopback TCP accepts local connections" :
					   loopback_tcp_failure_step);
	}
	orlix_test_result(loopback_udp_exchanges_datagram(),
			  "loopback UDP exchanges local datagrams");

	orlix_test_exit();
}
