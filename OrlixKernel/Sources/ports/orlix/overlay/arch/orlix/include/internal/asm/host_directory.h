/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ORLIX_INTERNAL_ASM_HOST_DIRECTORY_H
#define _ORLIX_INTERNAL_ASM_HOST_DIRECTORY_H

#include <linux/types.h>

#define ORLIX_HOST_DIRECTORY_NAME_MAX 255

enum orlix_host_directory_entry_type {
	ORLIX_HOST_DIRECTORY_ENTRY_UNKNOWN = 0,
	ORLIX_HOST_DIRECTORY_ENTRY_REGULAR = 1,
	ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY = 2,
	ORLIX_HOST_DIRECTORY_ENTRY_SYMLINK = 3,
};

struct orlix_host_directory_entry {
	u64 inode;
	u64 size;
	u32 mode;
	u8 type;
	char name[ORLIX_HOST_DIRECTORY_NAME_MAX + 1];
};

int orlix_host_directory_read_entry(unsigned int directory,
				    unsigned int entry_index,
				    struct orlix_host_directory_entry *entry);
int orlix_host_directory_read_child_entry(
				    unsigned int directory,
				    unsigned int parent_entry_index,
				    unsigned int entry_index,
				    struct orlix_host_directory_entry *entry);
long orlix_host_directory_read_file(unsigned int directory,
				    unsigned int entry_index,
				    u64 offset,
				    void *buffer,
				    u32 length);
long orlix_host_directory_read_child_file(unsigned int directory,
					  unsigned int parent_entry_index,
					  unsigned int entry_index,
					  u64 offset,
					  void *buffer,
					  u32 length);
long orlix_host_directory_read_link(unsigned int directory,
				    unsigned int entry_index,
				    void *buffer,
				    u32 length);
long orlix_host_directory_read_child_link(unsigned int directory,
					  unsigned int parent_entry_index,
					  unsigned int entry_index,
					  void *buffer,
					  u32 length);
long orlix_host_directory_list_xattr(unsigned int directory,
				     const char *relative_path,
				     char *buffer,
				     u64 capacity);
long orlix_host_directory_read_xattr(unsigned int directory,
				     const char *relative_path,
				     const char *name,
				     void *buffer,
				     u64 capacity);

#endif /* _ORLIX_INTERNAL_ASM_HOST_DIRECTORY_H */
