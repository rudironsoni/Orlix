/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INTERNAL_ASM_ORLIX_HOST_MEMORY_H
#define _INTERNAL_ASM_ORLIX_HOST_MEMORY_H

int orlix_host_kernel_map_page(unsigned long target_address,
			       const void *source_page,
			       unsigned long length);
int orlix_host_kernel_reserve_window(unsigned long minimum_address,
				     unsigned long maximum_address,
				     unsigned long length,
				     unsigned long alignment,
				     unsigned long *base_address);
void orlix_host_kernel_unmap_pages(unsigned long target_address,
				   unsigned long length);
unsigned long orlix_host_memory_page_size(void);
struct orlix_host_user_page_segment {
	unsigned long target_address;
	const void *source_page;
	unsigned long length;
	int writable;
	int executable;
};

enum orlix_host_user_mapping_failure_operation {
	ORLIX_HOST_USER_MAPPING_FAILURE_NONE = 0,
	ORLIX_HOST_USER_MAPPING_FAILURE_INVALID_ARGUMENT = 1,
	ORLIX_HOST_USER_MAPPING_FAILURE_CREATE_MAPPING = 2,
	ORLIX_HOST_USER_MAPPING_FAILURE_MISSING_MAPPING = 3,
	ORLIX_HOST_USER_MAPPING_FAILURE_COPY_PROTECT = 4,
	ORLIX_HOST_USER_MAPPING_FAILURE_CLEAR_PROTECT = 5,
	ORLIX_HOST_USER_MAPPING_FAILURE_SEGMENT_PROTECT = 6,
};

struct orlix_host_user_mapping_failure {
	unsigned long operation;
	unsigned long target_address;
	unsigned long length;
	unsigned long mapping_address;
	unsigned long mapping_length;
	unsigned long requested_protection;
	unsigned long attempted_protection;
	long host_status;
};

int orlix_host_user_map_page(unsigned long target_address,
			     const void *source_page,
			     unsigned long length,
			     int writable,
			     int executable);
int orlix_host_user_map_trusted_executable_page(unsigned long target_address,
						const void *source_page,
						unsigned long length);
int orlix_host_user_refresh_page(unsigned long target_address,
				 const void *source_page,
				 unsigned long length,
				 int writable,
				 int executable);
int orlix_host_user_refresh_page_range(unsigned long target_address,
				       const void *source_page,
				       unsigned long page_length,
				       unsigned long range_offset,
				       unsigned long range_length,
				       int writable,
				       int executable);
int orlix_host_user_refresh_window(
	unsigned long target_address,
	unsigned long length,
	const struct orlix_host_user_page_segment *segments,
	unsigned long segment_count);
int orlix_host_user_mapping_last_failure(
	struct orlix_host_user_mapping_failure *failure);
void orlix_host_user_unmap_pages(unsigned long target_address,
				 unsigned long length);
void orlix_host_user_discard_pages(unsigned long target_address,
				   unsigned long length);
void orlix_host_user_sync_writable_mappings(void);
void *orlix_host_ioremap(unsigned long physical_address,
			 unsigned long length);
void orlix_host_iounmap(void *mapped_address);
int orlix_host_iomem_physical_address(const void *mapped_address,
				      unsigned long *physical_address);

#endif /* _INTERNAL_ASM_ORLIX_HOST_MEMORY_H */
