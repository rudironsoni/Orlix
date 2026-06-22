// SPDX-License-Identifier: GPL-2.0
#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "orlix_kselftest_user.h"

static char property_data[512];
static char property_path_buffer[128];

static void build_property_path(const char *node, const char *property)
{
	const char prefix[] = "/proc/device-tree/";
	size_t pos = 0;
	size_t i;

	for (i = 0; prefix[i]; i++)
		property_path_buffer[pos++] = prefix[i];
	for (i = 0; node[i]; i++)
		property_path_buffer[pos++] = node[i];
	property_path_buffer[pos++] = '/';
	for (i = 0; property[i]; i++)
		property_path_buffer[pos++] = property[i];
	property_path_buffer[pos] = '\0';
}

static bool read_property(const char *node, const char *property, size_t *size)
{
	build_property_path(node, property);
	return orlix_read_file(property_path_buffer, property_data,
			       sizeof(property_data), size) == 0;
}

static bool property_contains_string(const char *node, const char *property,
				     const char *expected)
{
	size_t size;

	if (!read_property(node, property, &size))
		return false;
	return orlix_contains(property_data, size, expected);
}

static bool property_matches_u32_cells(const char *node, const char *property,
				       const uint32_t *expected,
				       size_t expected_count)
{
	size_t size;
	size_t i;

	if (!read_property(node, property, &size))
		return false;
	if (size != expected_count * sizeof(uint32_t))
		return false;

	for (i = 0; i < expected_count; i++) {
		const unsigned char *cell =
			(const unsigned char *)property_data + i * sizeof(uint32_t);

		if (orlix_read_be32(cell) != expected[i])
			return false;
	}
	return true;
}

static void expect_virtio_mmio_node(const char *node, uint32_t address,
				    uint32_t interrupt)
{
	uint32_t reg[] = { 0x0, address, 0x0, 0x200 };
	uint32_t interrupts[] = { interrupt };

	orlix_test_result(property_contains_string(node, "compatible", "virtio,mmio"),
			  "virtio-mmio node uses upstream compatible string");
	orlix_test_result(property_matches_u32_cells(node, "reg", reg, 4),
			  "virtio-mmio node has expected register range");
	orlix_test_result(property_matches_u32_cells(node, "interrupts",
						     interrupts, 1),
			  "virtio-mmio node has expected interrupt");
}

static bool hwrng_device_returns_data(void)
{
	unsigned char buffer[32];
	int fd = open("/dev/hwrng", O_RDONLY | O_NONBLOCK);
	ssize_t nread;

	if (fd < 0)
		return false;

	nread = read(fd, buffer, sizeof(buffer));
	close(fd);
	return nread > 0;
}

static bool virtio_device_name(const char *name)
{
	return name[0] == 'v' && name[1] == 'i' && name[2] == 'r' &&
	       name[3] == 't' && name[4] == 'i' && name[5] == 'o';
}

static bool build_virtio_device_path(char *path, size_t path_size,
				     const char *name, const char *leaf)
{
	const char prefix[] = "/sys/bus/virtio/devices/";
	size_t pos = 0;

	for (size_t i = 0; prefix[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = prefix[i];
	}
	for (size_t i = 0; name[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = name[i];
	}
	if (pos + 1 >= path_size)
		return false;
	path[pos++] = '/';
	for (size_t i = 0; leaf[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = leaf[i];
	}
	path[pos] = '\0';
	return true;
}

static bool build_virtio_device_id_path(char *path, size_t path_size,
					const char *name)
{
	return build_virtio_device_path(path, path_size, name, "device");
}

static bool build_virtio_tag_path(char *path, size_t path_size,
				  const char *name)
{
	return build_virtio_device_path(path, path_size, name, "tag");
}

static bool build_virtiofs_tag_path(char *path, size_t path_size,
				    const char *name)
{
	const char prefix[] = "/sys/fs/virtiofs/";
	const char suffix[] = "/tag";
	size_t pos = 0;

	for (size_t i = 0; prefix[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = prefix[i];
	}
	for (size_t i = 0; name[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = name[i];
	}
	for (size_t i = 0; suffix[i] != '\0'; i++) {
		if (pos + 1 >= path_size)
			return false;
		path[pos++] = suffix[i];
	}
	path[pos] = '\0';
	return true;
}

static bool virtio_bus_has_device(void)
{
	DIR *devices = opendir("/sys/bus/virtio/devices");
	struct dirent *entry;
	bool found = false;

	if (!devices)
		return false;
	while ((entry = readdir(devices)) != NULL) {
		if (virtio_device_name(entry->d_name)) {
			found = true;
			break;
		}
	}
	closedir(devices);
	return found;
}

static bool virtio_bus_has_fs_device(void)
{
	DIR *devices = opendir("/sys/bus/virtio/devices");
	struct dirent *entry;
	char path[128];
	char buffer[64];
	size_t size = 0;
	bool found = false;

	if (!devices)
		return false;
	while ((entry = readdir(devices)) != NULL) {
		if (!virtio_device_name(entry->d_name))
			continue;
		if (!build_virtio_device_id_path(path, sizeof(path),
						 entry->d_name))
			continue;
		if (orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
		    (orlix_contains(buffer, size, "001a") ||
		     orlix_contains(buffer, size, "26"))) {
			found = true;
			break;
		}
	}
	closedir(devices);
	return found;
}

static bool virtiofs_tag_is_registered(void)
{
	DIR *devices = opendir("/sys/fs/virtiofs");
	struct dirent *entry;
	char path[128];
	char buffer[64];
	size_t size = 0;

	if (!devices)
		return false;

	while ((entry = readdir(devices)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		orlix_write_all("# virtiofs instance ");
		orlix_write_all(entry->d_name);
		orlix_write_all("\n");
		if (!build_virtiofs_tag_path(path, sizeof(path), entry->d_name))
			continue;
		if (orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
		    orlix_contains(buffer, size, "orlix-host0")) {
			orlix_write_all("# virtiofs tag path ");
			orlix_write_all(path);
			orlix_write_all("\n");
			closedir(devices);
			return true;
		}
	}

	closedir(devices);
	return false;
}

static bool virtio_bus_has_device_id(const char *hex_id, const char *decimal_id)
{
	DIR *devices = opendir("/sys/bus/virtio/devices");
	struct dirent *entry;
	bool found = false;

	if (!devices)
		return false;

	while ((entry = readdir(devices)) != NULL) {
		const char prefix[] = "/sys/bus/virtio/devices/";
		const char suffix[] = "/device";
		char path[128];
		char buffer[64];
		size_t size;
		size_t pos = 0;
		size_t i;
		bool overflow = false;

		if (entry->d_name[0] == '.')
			continue;

		for (i = 0; prefix[i] != '\0' && pos + 1 < sizeof(path); i++)
			path[pos++] = prefix[i];
		if (prefix[i] != '\0')
			overflow = true;
		for (i = 0; entry->d_name[i] != '\0' && pos + 1 < sizeof(path);
		     i++)
			path[pos++] = entry->d_name[i];
		if (entry->d_name[i] != '\0')
			overflow = true;
		for (i = 0; suffix[i] != '\0' && pos + 1 < sizeof(path); i++)
			path[pos++] = suffix[i];
		if (suffix[i] != '\0')
			overflow = true;
		if (overflow)
			continue;
		path[pos] = '\0';

		if (orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
		    (orlix_contains(buffer, size, hex_id) ||
		     orlix_contains(buffer, size, decimal_id))) {
			found = true;
			break;
		}
	}

	closedir(devices);
	return found;
}

int main(void)
{
	orlix_test_plan(23);

	expect_virtio_mmio_node("virtio@10001000", 0x10001000, 32);
	expect_virtio_mmio_node("virtio@10001200", 0x10001200, 33);
	expect_virtio_mmio_node("virtio@10001400", 0x10001400, 34);
	expect_virtio_mmio_node("virtio@10001600", 0x10001600, 35);
	expect_virtio_mmio_node("virtio@10001800", 0x10001800, 36);
	expect_virtio_mmio_node("virtio@10001a00", 0x10001a00, 37);

	orlix_test_result(hwrng_device_returns_data(),
			  "upstream hwrng device returns virtio-backed entropy");
	orlix_test_result(virtio_bus_has_device(),
			  "upstream virtio bus exposes devices");
	orlix_test_result(virtio_bus_has_device_id("0001", "1"),
			  "upstream virtio bus exposes the virtio-net device");
	orlix_test_result(virtio_bus_has_fs_device(),
			  "upstream virtio bus exposes the Orlix virtio-fs device");
	orlix_test_result(virtiofs_tag_is_registered(),
			  "upstream virtio-fs device registers the Orlix host-folder tag orlix-host0");

	orlix_test_exit();
}
