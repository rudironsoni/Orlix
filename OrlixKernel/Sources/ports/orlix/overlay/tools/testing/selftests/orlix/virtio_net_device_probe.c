// SPDX-License-Identifier: GPL-2.0

#include <dirent.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_ARPHRD_ETHER 1
#define ORLIX_ETH_ALEN 6

static bool read_text_file(const char *path, char *buffer, size_t size)
{
	int fd;
	ssize_t received;

	if (size == 0)
		return false;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	received = read(fd, buffer, size - 1);
	close(fd);

	if (received < 0)
		return false;

	buffer[received] = '\0';
	return true;
}

static void trim_trailing_newline(char *text)
{
	size_t len = strlen(text);

	while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
		text[--len] = '\0';
}

static bool append_path(char *buffer, size_t size, const char *prefix,
			const char *name, const char *suffix)
{
	size_t prefix_len = strlen(prefix);
	size_t name_len = strlen(name);
	size_t suffix_len = strlen(suffix);

	if (prefix_len + name_len + suffix_len + 1 > size)
		return false;

	memcpy(buffer, prefix, prefix_len);
	memcpy(buffer + prefix_len, name, name_len);
	memcpy(buffer + prefix_len + name_len, suffix, suffix_len);
	buffer[prefix_len + name_len + suffix_len] = '\0';
	return true;
}

static bool find_virtio_net_device(char *device_name, size_t device_name_size)
{
	DIR *devices;
	struct dirent *entry;

	devices = opendir("/sys/bus/virtio/devices");
	if (!devices)
		return false;

	while ((entry = readdir(devices)) != NULL) {
		char path[160];
		char device_id[64];

		if (entry->d_name[0] == '.')
			continue;

		if (!append_path(path, sizeof(path),
				 "/sys/bus/virtio/devices/", entry->d_name,
				 "/device"))
			continue;

		if (!read_text_file(path, device_id, sizeof(device_id)))
			continue;

		trim_trailing_newline(device_id);
		if (strcmp(device_id, "0x0001") != 0 &&
		    strcmp(device_id, "0001") != 0 &&
		    strcmp(device_id, "1") != 0)
			continue;

		if (strlen(entry->d_name) + 1 > device_name_size)
			continue;

		strcpy(device_name, entry->d_name);
		closedir(devices);
		return true;
	}

	closedir(devices);
	return false;
}

static bool find_netdev_for_virtio_device(const char *device_name,
					  char *ifname, size_t ifname_size)
{
	char net_path[160];
	DIR *netdevs;
	struct dirent *entry;

	if (!append_path(net_path, sizeof(net_path),
			 "/sys/bus/virtio/devices/", device_name, "/net"))
		return false;

	netdevs = opendir(net_path);
	if (!netdevs)
		return false;

	while ((entry = readdir(netdevs)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		if (strcmp(entry->d_name, "lo") == 0)
			continue;

		if (strlen(entry->d_name) + 1 > ifname_size)
			continue;

		strcpy(ifname, entry->d_name);
		closedir(netdevs);
		return true;
	}

	closedir(netdevs);
	return false;
}

static bool sysfs_netdev_exists(const char *ifname)
{
	char path[128];
	DIR *netdev;

	if (!append_path(path, sizeof(path), "/sys/class/net/", ifname, ""))
		return false;

	netdev = opendir(path);
	if (!netdev)
		return false;

	closedir(netdev);
	return true;
}

static bool read_sysfs_ulong(const char *path, unsigned long *value)
{
	char buffer[64];
	size_t index = 0;
	unsigned long parsed = 0;

	if (!read_text_file(path, buffer, sizeof(buffer)))
		return false;

	if (buffer[0] < '0' || buffer[0] > '9')
		return false;

	while (buffer[index] >= '0' && buffer[index] <= '9') {
		parsed = parsed * 10 + (unsigned long)(buffer[index] - '0');
		++index;
	}

	*value = parsed;
	return true;
}

static bool sysfs_netdev_reports_ethernet_type(const char *ifname)
{
	char path[128];
	unsigned long type = 0;

	if (!append_path(path, sizeof(path), "/sys/class/net/", ifname, "/type"))
		return false;

	return read_sysfs_ulong(path, &type) && type == ORLIX_ARPHRD_ETHER;
}

static bool sysfs_netdev_reports_ethernet_addr_len(const char *ifname)
{
	char path[128];
	unsigned long addr_len = 0;

	if (!append_path(path, sizeof(path), "/sys/class/net/", ifname,
			 "/addr_len"))
		return false;

	return read_sysfs_ulong(path, &addr_len) && addr_len == ORLIX_ETH_ALEN;
}

static bool sysfs_netdev_reports_mtu(const char *ifname, unsigned long *mtu)
{
	char path[128];

	if (!append_path(path, sizeof(path), "/sys/class/net/", ifname, "/mtu"))
		return false;

	return read_sysfs_ulong(path, mtu) && *mtu > 0;
}

static bool rtnetlink_reports_link(const char *expected_ifname,
				   unsigned long expected_mtu)
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
	bool saw_link = false;
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
			bool name_matches = false;
			bool has_ethernet_address = false;
			bool mtu_matches = false;

			if (message->nlmsg_type == NLMSG_DONE) {
				close(fd);
				return saw_link;
			}

			if (message->nlmsg_type == NLMSG_ERROR) {
				close(fd);
				return false;
			}

			if (message->nlmsg_type != RTM_NEWLINK)
				continue;

			interface = NLMSG_DATA(message);
			if (interface->ifi_index <= 0 ||
			    (interface->ifi_flags & IFF_LOOPBACK))
				continue;

			attributes_len = IFLA_PAYLOAD(message);
			for (attribute = IFLA_RTA(interface);
			     RTA_OK(attribute, attributes_len);
			     attribute = RTA_NEXT(attribute, attributes_len)) {
				if (attribute->rta_type == IFLA_IFNAME) {
					const char *ifname = RTA_DATA(attribute);

					if (strcmp(ifname, expected_ifname) == 0)
						name_matches = true;
				}

				if (attribute->rta_type == IFLA_ADDRESS &&
				    RTA_PAYLOAD(attribute) == ORLIX_ETH_ALEN) {
					const unsigned char *mac = RTA_DATA(attribute);
					size_t mac_index;
					bool has_nonzero_octet = false;

					for (mac_index = 0; mac_index < ORLIX_ETH_ALEN; ++mac_index)
						has_nonzero_octet |= mac[mac_index] != 0;

					has_ethernet_address = has_nonzero_octet;
				}
				if (attribute->rta_type == IFLA_MTU &&
				    RTA_PAYLOAD(attribute) == sizeof(unsigned int)) {
					unsigned int mtu = 0;

					memcpy(&mtu, RTA_DATA(attribute), sizeof(mtu));
					mtu_matches = mtu == expected_mtu;
				}
			}

			if (name_matches && has_ethernet_address && mtu_matches)
				saw_link = true;
		}
	}
}

static bool proc_net_dev_reports_interface(const char *ifname)
{
	char buffer[4096];

	if (!read_text_file("/proc/net/dev", buffer, sizeof(buffer)))
		return false;

	return orlix_contains(buffer, strlen(buffer), ifname);
}

int main(void)
{
	char device_name[64] = { 0 };
	char ifname[IFNAMSIZ] = { 0 };
	unsigned long mtu = 0;
	bool device_present;
	bool owns_netdev = false;
	bool has_mtu = false;

	orlix_test_plan(9);

	device_present = find_virtio_net_device(device_name, sizeof(device_name));
	orlix_test_result(device_present,
			  "virtio-net device is present on the upstream virtio bus");

	if (device_present)
		owns_netdev = find_netdev_for_virtio_device(device_name, ifname,
							    sizeof(ifname));
	orlix_test_result(owns_netdev,
			  "virtio-net device owns a Linux netdev");

	orlix_test_result(owns_netdev && sysfs_netdev_exists(ifname),
			  "virtio-net netdev is exposed through sysfs");
	orlix_test_result(owns_netdev && sysfs_netdev_reports_ethernet_type(ifname),
			  "virtio-net netdev reports Ethernet hardware type");
	orlix_test_result(owns_netdev && sysfs_netdev_reports_ethernet_addr_len(ifname),
			  "virtio-net netdev reports Ethernet address length");
	has_mtu = owns_netdev && sysfs_netdev_reports_mtu(ifname, &mtu);
	orlix_test_result(has_mtu,
			  "virtio-net netdev reports a positive MTU through sysfs");
	orlix_test_result(has_mtu && rtnetlink_reports_link(ifname, mtu),
			  "rtnetlink enumerates the virtio-net Ethernet link with matching MTU");
	orlix_test_result(owns_netdev && strcmp(ifname, "lo") != 0,
			  "virtio-net link is distinct from loopback");
	orlix_test_result(owns_netdev && proc_net_dev_reports_interface(ifname),
			  "procfs reports the virtio-net interface");

	orlix_test_exit();
}
