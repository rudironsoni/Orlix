// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/limits.h>
#include <linux/stat.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/virtio_config.h>
#include <linux/workqueue.h>
#include <linux/virtio_ids.h>
#include <uapi/linux/fuse.h>
#include <uapi/linux/virtio_blk.h>
#include <uapi/linux/virtio_fs.h>
#include <uapi/linux/virtio_mmio.h>
#include <uapi/linux/virtio_ring.h>
#include <asm/page.h>
#include <internal/asm/host_block.h>
#include <internal/asm/host_directory.h>
#include <internal/asm/host_console.h>
#include <internal/asm/host_entropy.h>
#include <internal/asm/irq.h>
#include <internal/asm/virtio_mmio.h>

#define ORLIX_VIRTIO_MMIO_MAGIC ('v' | ('i' << 8) | ('r' << 16) | ('t' << 24))
#define ORLIX_VIRTIO_MMIO_VENDOR 0x4f524c58U
#define ORLIX_VIRTIO_MMIO_SLOT_SIZE 0x200UL
#define ORLIX_VIRTIO_MMIO_QUEUE_COUNT 2
#define ORLIX_VIRTIO_MMIO_QUEUE_SIZE 128
#define ORLIX_VIRTIO_MMIO_BLOCK_SECTOR_SIZE 512ULL
#define ORLIX_VIRTIO_MMIO_CONSOLE_INPUT_POLL_MS 10
#define ORLIX_VIRTIO_MMIO_FUSE_ACCESS_W_OK 2
#define ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY 0
#define ORLIX_VIRTIO_MMIO_FS_HOST_NODE_BASE 3
#define ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT 1024
#define ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_BASE 4096
#define ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE 1024
#define ORLIX_VIRTIO_BLK_BASE_FEATURES \
	((1ULL << VIRTIO_F_VERSION_1) | (1ULL << VIRTIO_RING_F_INDIRECT_DESC) | \
	 (1ULL << VIRTIO_BLK_F_FLUSH))
#define ORLIX_VIRTIO_CONSOLE_BASE_FEATURES (1ULL << VIRTIO_F_VERSION_1)
#define ORLIX_VIRTIO_RNG_BASE_FEATURES (1ULL << VIRTIO_F_VERSION_1)
#define ORLIX_VIRTIO_FS_BASE_FEATURES (1ULL << VIRTIO_F_VERSION_1)
#define ORLIX_VIRTIO_FS_REQUEST_QUEUES 1U

struct orlix_virtio_mmio_queue {
	u32 num;
	u32 ready;
	u64 desc;
	u64 avail;
	u64 used;
	u16 last_avail;
};

struct orlix_virtio_mmio_slot {
	unsigned long base;
	unsigned int irq;
	u32 device_id;
	unsigned int host_block_device;
	bool read_only;
	const char *device_identifier;
	u32 status;
	u32 interrupt_status;
	u32 device_features_sel;
	u32 driver_features_sel;
	u32 driver_features[2];
	u32 queue_sel;
	u32 shm_sel;
	struct orlix_virtio_mmio_queue queues[ORLIX_VIRTIO_MMIO_QUEUE_COUNT];
	struct work_struct notify_work;
	unsigned long pending_queues;
	bool notify_work_initialized;
	struct timer_list console_input_timer;
	bool console_input_timer_initialized;
};

struct orlix_virtio_mmio_desc_chain {
	struct vring_desc *desc;
	unsigned int head;
	unsigned int count;
};

static struct orlix_virtio_mmio_slot orlix_virtio_mmio_slots[] = {
	{
		.base = 0x10001000UL,
		.irq = 32,
		.device_id = VIRTIO_ID_BLOCK,
		.host_block_device = 0,
		.read_only = true,
		.device_identifier = "orlix-base-block0",
	},
	{
		.base = 0x10001200UL,
		.irq = 33,
		.device_id = VIRTIO_ID_BLOCK,
		.host_block_device = 1,
		.device_identifier = "orlix-state-block1",
	},
	{
		.base = 0x10001400UL,
		.irq = 34,
		.device_id = VIRTIO_ID_CONSOLE,
		.device_identifier = "orlix-console0",
	},
	{
		.base = 0x10001600UL,
		.irq = 35,
		.device_id = VIRTIO_ID_RNG,
		.device_identifier = "orlix-rng0",
	},
	{
		.base = 0x10001800UL,
		.irq = 36,
		.device_id = VIRTIO_ID_FS,
		.device_identifier = "orlix-host0",
	},
};

static struct orlix_virtio_mmio_slot *
orlix_virtio_mmio_find_slot(unsigned long physical_address,
			    unsigned long *register_offset)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_virtio_mmio_slots); index++) {
		struct orlix_virtio_mmio_slot *slot =
			&orlix_virtio_mmio_slots[index];

		if (physical_address >= slot->base &&
		    physical_address < slot->base + ORLIX_VIRTIO_MMIO_SLOT_SIZE) {
			*register_offset = physical_address - slot->base;
			return slot;
		}
	}

	return NULL;
}

static struct orlix_virtio_mmio_queue *
orlix_virtio_mmio_selected_queue(struct orlix_virtio_mmio_slot *slot)
{
	if (slot->queue_sel >= ARRAY_SIZE(slot->queues))
		return NULL;

	return &slot->queues[slot->queue_sel];
}

static bool
orlix_virtio_mmio_block_capacity(const struct orlix_virtio_mmio_slot *slot,
				 unsigned long long *sectors)
{
	return slot->device_id == VIRTIO_ID_BLOCK &&
	       orlix_host_block_capacity(slot->host_block_device, sectors) == 0;
}

static bool
orlix_virtio_mmio_device_present(const struct orlix_virtio_mmio_slot *slot)
{
	unsigned long long sectors;

	switch (slot->device_id) {
	case VIRTIO_ID_BLOCK:
		return orlix_virtio_mmio_block_capacity(slot, &sectors);
	case VIRTIO_ID_CONSOLE:
	case VIRTIO_ID_RNG:
	case VIRTIO_ID_FS:
		return true;
	default:
		return false;
	}
}

static u64 orlix_virtio_mmio_device_features(
	const struct orlix_virtio_mmio_slot *slot)
{
	unsigned long long sectors;

	switch (slot->device_id) {
	case VIRTIO_ID_BLOCK:
		if (!orlix_virtio_mmio_block_capacity(slot, &sectors))
			return 0;
		return ORLIX_VIRTIO_BLK_BASE_FEATURES |
		       (slot->read_only ? (1ULL << VIRTIO_BLK_F_RO) : 0);
	case VIRTIO_ID_CONSOLE:
		return ORLIX_VIRTIO_CONSOLE_BASE_FEATURES;
	case VIRTIO_ID_RNG:
		return ORLIX_VIRTIO_RNG_BASE_FEATURES;
	case VIRTIO_ID_FS:
		return ORLIX_VIRTIO_FS_BASE_FEATURES;
	default:
		return 0;
	}
}

static u32 orlix_virtio_mmio_fs_config_read32(
	const struct orlix_virtio_mmio_slot *slot,
	unsigned long config_offset)
{
	u8 bytes[sizeof(u32)] = {};
	__le32 request_queues = cpu_to_le32(ORLIX_VIRTIO_FS_REQUEST_QUEUES);
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bytes); i++) {
		unsigned long offset = config_offset + i;

		if (offset < sizeof_field(struct virtio_fs_config, tag)) {
			if (offset < strlen(slot->device_identifier))
				bytes[i] = slot->device_identifier[offset];
			continue;
		}

		offset -= sizeof_field(struct virtio_fs_config, tag);
		if (offset < sizeof(request_queues))
			bytes[i] = ((u8 *)&request_queues)[offset];
	}

	return (u32)bytes[0] | ((u32)bytes[1] << 8) |
	       ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static u32 orlix_virtio_mmio_config_read32(
	const struct orlix_virtio_mmio_slot *slot,
	unsigned long config_offset)
{
	unsigned long long sectors;

	switch (slot->device_id) {
	case VIRTIO_ID_BLOCK:
		if (!orlix_virtio_mmio_block_capacity(slot, &sectors))
			return 0;
		if (config_offset == offsetof(struct virtio_blk_config, capacity))
			return (u32)sectors;
		if (config_offset ==
		    offsetof(struct virtio_blk_config, capacity) + sizeof(u32))
			return (u32)(sectors >> 32);
		return 0;
	case VIRTIO_ID_FS:
		return orlix_virtio_mmio_fs_config_read32(slot, config_offset);
	default:
		return 0;
	}
}

static void *orlix_virtio_mmio_guest_ptr(u64 address, u32 length)
{
	if (!address || address > ULONG_MAX || length > ULONG_MAX - address)
		return NULL;

	return __va((unsigned long)address);
}

static u16 orlix_vring_read16(__virtio16 value)
{
	return le16_to_cpu((__force __le16)value);
}

static u32 orlix_vring_read32(__virtio32 value)
{
	return le32_to_cpu((__force __le32)value);
}

static u64 orlix_vring_read64(__virtio64 value)
{
	return le64_to_cpu((__force __le64)value);
}

static u32 orlix_virtio_mmio_block_command(u32 type)
{
	return type & ~VIRTIO_BLK_T_BARRIER;
}

static void orlix_vring_write16(__virtio16 *target, u16 value)
{
	*target = (__force __virtio16)cpu_to_le16(value);
}

static void orlix_vring_write32(__virtio32 *target, u32 value)
{
	*target = (__force __virtio32)cpu_to_le32(value);
}

static bool orlix_virtio_mmio_resolve_desc_chain(
	struct vring_desc *queue_desc,
	unsigned int queue_num,
	unsigned int head,
	struct orlix_virtio_mmio_desc_chain *chain)
{
	u16 flags;
	u32 length;
	void *indirect;

	if (!chain || head >= queue_num)
		return false;

	flags = orlix_vring_read16(queue_desc[head].flags);
	if (!(flags & VRING_DESC_F_INDIRECT)) {
		chain->desc = queue_desc;
		chain->head = head;
		chain->count = queue_num;
		return true;
	}

	length = orlix_vring_read32(queue_desc[head].len);
	if (!length || length % sizeof(struct vring_desc))
		return false;

	indirect = orlix_virtio_mmio_guest_ptr(
		orlix_vring_read64(queue_desc[head].addr), length);
	if (!indirect)
		return false;

	chain->desc = indirect;
	chain->head = 0;
	chain->count = length / sizeof(struct vring_desc);
	return chain->count > 0;
}

static u8 orlix_virtio_mmio_process_block_data(
	const struct orlix_virtio_mmio_slot *slot,
	u32 type,
	u64 sector,
	const struct vring_desc *desc,
	unsigned int desc_index,
	unsigned int *written)
{
	void *buffer;
	u32 length = orlix_vring_read32(desc[desc_index].len);
	u16 flags = orlix_vring_read16(desc[desc_index].flags);
	u64 address = orlix_vring_read64(desc[desc_index].addr);

	if (flags & VRING_DESC_F_INDIRECT)
		return VIRTIO_BLK_S_IOERR;

	buffer = orlix_virtio_mmio_guest_ptr(address, length);
	if (!buffer)
		return VIRTIO_BLK_S_IOERR;

	switch (type) {
	case VIRTIO_BLK_T_IN:
		if (!(flags & VRING_DESC_F_WRITE))
			return VIRTIO_BLK_S_IOERR;
		if (orlix_host_block_read(slot->host_block_device, sector,
					  buffer, length) != 0)
			return VIRTIO_BLK_S_IOERR;
		*written += length;
		return VIRTIO_BLK_S_OK;
	case VIRTIO_BLK_T_OUT:
		if (flags & VRING_DESC_F_WRITE)
			return VIRTIO_BLK_S_IOERR;
		if (orlix_host_block_write(slot->host_block_device, sector,
					   buffer, length) != 0)
			return VIRTIO_BLK_S_IOERR;
		return VIRTIO_BLK_S_OK;
	case VIRTIO_BLK_T_GET_ID:
		if (!(flags & VRING_DESC_F_WRITE))
			return VIRTIO_BLK_S_IOERR;
		memset(buffer, 0, length);
		strscpy(buffer, slot->device_identifier, length);
		*written += length;
		return VIRTIO_BLK_S_OK;
	default:
		return VIRTIO_BLK_S_UNSUPP;
	}
}

static u8 orlix_virtio_mmio_process_block_request(
	const struct orlix_virtio_mmio_slot *slot,
	struct vring_desc *desc,
	unsigned int head,
	unsigned int desc_count,
	unsigned int *written)
{
	struct orlix_virtio_mmio_desc_chain chain;
	struct virtio_blk_outhdr *header;
	u16 flags;
	u32 type;
	u64 sector;
	unsigned int descriptor_index = head;
	unsigned int status_index;
	unsigned int guard;
	u8 *status;
	u8 request_status = VIRTIO_BLK_S_OK;

	if (!orlix_virtio_mmio_resolve_desc_chain(desc, desc_count, head, &chain))
		return VIRTIO_BLK_S_IOERR;

	desc = chain.desc;
	descriptor_index = chain.head;

	flags = orlix_vring_read16(desc[descriptor_index].flags);
	if ((flags & (VRING_DESC_F_WRITE | VRING_DESC_F_INDIRECT)) ||
	    !(flags & VRING_DESC_F_NEXT))
		return VIRTIO_BLK_S_IOERR;

	header = orlix_virtio_mmio_guest_ptr(
		orlix_vring_read64(desc[descriptor_index].addr),
		orlix_vring_read32(desc[descriptor_index].len));
	if (!header)
		return VIRTIO_BLK_S_IOERR;

	type = orlix_vring_read32(header->type);
	type = orlix_virtio_mmio_block_command(type);
	sector = orlix_vring_read64(header->sector);
	descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
	if (descriptor_index >= chain.count)
		return VIRTIO_BLK_S_IOERR;

	for (guard = 0; guard < chain.count; guard++) {
		if (descriptor_index >= chain.count)
			return VIRTIO_BLK_S_IOERR;

		flags = orlix_vring_read16(desc[descriptor_index].flags);
		if (flags & VRING_DESC_F_INDIRECT)
			return VIRTIO_BLK_S_IOERR;
		if (!(flags & VRING_DESC_F_NEXT))
			break;

		if (request_status == VIRTIO_BLK_S_OK) {
			u8 result = orlix_virtio_mmio_process_block_data(
				slot, type, sector, desc, descriptor_index, written);
			if (result != VIRTIO_BLK_S_OK)
				request_status = result;
		}

		if (request_status == VIRTIO_BLK_S_OK)
			sector += orlix_vring_read32(desc[descriptor_index].len) /
				  ORLIX_VIRTIO_MMIO_BLOCK_SECTOR_SIZE;
		descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
	}

	if (guard == chain.count)
		return VIRTIO_BLK_S_IOERR;

	status_index = descriptor_index;
	flags = orlix_vring_read16(desc[status_index].flags);
	if (!(flags & VRING_DESC_F_WRITE) ||
	    (flags & (VRING_DESC_F_NEXT | VRING_DESC_F_INDIRECT)) ||
	    orlix_vring_read32(desc[status_index].len) < 1)
		return VIRTIO_BLK_S_IOERR;

	status = orlix_virtio_mmio_guest_ptr(
		orlix_vring_read64(desc[status_index].addr), 1);
	if (!status)
		return VIRTIO_BLK_S_IOERR;

	if (type != VIRTIO_BLK_T_IN && type != VIRTIO_BLK_T_OUT &&
	    type != VIRTIO_BLK_T_GET_ID && type != VIRTIO_BLK_T_FLUSH)
		request_status = VIRTIO_BLK_S_UNSUPP;
	if (type == VIRTIO_BLK_T_FLUSH &&
	    orlix_host_block_flush(slot->host_block_device) != 0)
		request_status = VIRTIO_BLK_S_IOERR;

	*status = request_status;

	(*written)++;
	return request_status;
}

static void orlix_virtio_mmio_push_used(
	struct orlix_virtio_mmio_slot *slot,
	struct orlix_virtio_mmio_queue *queue,
	unsigned int head,
	unsigned int written)
{
	struct vring_used *used = orlix_virtio_mmio_guest_ptr(
		queue->used, sizeof(*used));
	u16 used_index;

	if (!used)
		return;

	used_index = orlix_vring_read16(used->idx);
	orlix_vring_write32(&used->ring[used_index % queue->num].id, head);
	orlix_vring_write32(&used->ring[used_index % queue->num].len, written);
	orlix_vring_write16(&used->idx, used_index + 1);
	slot->interrupt_status |= VIRTIO_MMIO_INT_VRING;
	orlix_irq_dispatch(slot->irq);
}

static bool orlix_virtio_mmio_process_console_input_desc(
	const struct vring_desc *desc,
	unsigned int head,
	unsigned int *written)
{
	unsigned int descriptor_index = head;
	unsigned int guard;

	if (head >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return true;

	for (guard = 0; guard < ORLIX_VIRTIO_MMIO_QUEUE_SIZE; guard++) {
		void *buffer;
		u32 length;
		u16 flags;
		u64 address;
		unsigned long copied;

		if (descriptor_index >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
			return true;

		length = orlix_vring_read32(desc[descriptor_index].len);
		flags = orlix_vring_read16(desc[descriptor_index].flags);
		address = orlix_vring_read64(desc[descriptor_index].addr);

		if (!(flags & VRING_DESC_F_WRITE))
			return true;

		buffer = orlix_virtio_mmio_guest_ptr(address, length);
		if (!buffer)
			return true;

		copied = orlix_host_console_read_input(buffer, length);
		*written += copied;
		if (copied < length)
			return *written > 0;

		if (!(flags & VRING_DESC_F_NEXT))
			return true;

		descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
	}

	return true;
}

static void orlix_virtio_mmio_process_console_input_queue(
	struct orlix_virtio_mmio_slot *slot)
{
	struct orlix_virtio_mmio_queue *queue = &slot->queues[0];
	struct vring_desc *desc;
	struct vring_avail *avail;

	if (!queue->ready || !queue->num || queue->num > ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	desc = orlix_virtio_mmio_guest_ptr(
		queue->desc, sizeof(struct vring_desc) * queue->num);
	avail = orlix_virtio_mmio_guest_ptr(queue->avail, sizeof(*avail));
	if (!desc || !avail)
		return;

	while (queue->last_avail != orlix_vring_read16(avail->idx)) {
		unsigned int head =
			orlix_vring_read16(avail->ring[queue->last_avail % queue->num]);
		unsigned int written = 0;

		if (!orlix_virtio_mmio_process_console_input_desc(desc, head,
								  &written))
			break;

		orlix_virtio_mmio_push_used(slot, queue, head, written);
		queue->last_avail++;
	}
}

static void orlix_virtio_mmio_console_input_timer(struct timer_list *timer)
{
	struct orlix_virtio_mmio_slot *slot =
		from_timer(slot, timer, console_input_timer);

	orlix_virtio_mmio_process_console_input_queue(slot);
	if (slot->queues[0].ready)
		mod_timer(&slot->console_input_timer,
			  jiffies + msecs_to_jiffies(ORLIX_VIRTIO_MMIO_CONSOLE_INPUT_POLL_MS));
}

static void orlix_virtio_mmio_start_console_input_timer(
	struct orlix_virtio_mmio_slot *slot)
{
	if (slot->device_id != VIRTIO_ID_CONSOLE)
		return;

	if (!slot->console_input_timer_initialized) {
		timer_setup(&slot->console_input_timer,
			    orlix_virtio_mmio_console_input_timer, 0);
		slot->console_input_timer_initialized = true;
	}

	mod_timer(&slot->console_input_timer,
		  jiffies + msecs_to_jiffies(ORLIX_VIRTIO_MMIO_CONSOLE_INPUT_POLL_MS));
}

static void orlix_virtio_mmio_process_console_output_desc(
	const struct vring_desc *desc,
	unsigned int head,
	unsigned int *written)
{
	unsigned int descriptor_index = head;
	unsigned int guard;

	if (head >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	for (guard = 0; guard < ORLIX_VIRTIO_MMIO_QUEUE_SIZE; guard++) {
		void *buffer;
		u32 length;
		u16 flags;
		u64 address;

		if (descriptor_index >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
			return;

		length = orlix_vring_read32(desc[descriptor_index].len);
		flags = orlix_vring_read16(desc[descriptor_index].flags);
		address = orlix_vring_read64(desc[descriptor_index].addr);

		if (flags & VRING_DESC_F_WRITE)
			return;

		buffer = orlix_virtio_mmio_guest_ptr(address, length);
		if (!buffer)
			return;

		orlix_host_console_write(buffer, length);
		*written += length;

		if (!(flags & VRING_DESC_F_NEXT))
			return;

		descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
	}
}

static void orlix_virtio_mmio_process_console_output_queue(
	struct orlix_virtio_mmio_slot *slot,
	u32 queue_index)
{
	struct orlix_virtio_mmio_queue *queue;
	struct vring_desc *desc;
	struct vring_avail *avail;

	if (queue_index >= ARRAY_SIZE(slot->queues))
		return;

	queue = &slot->queues[queue_index];
	if (!queue->ready || !queue->num || queue->num > ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	desc = orlix_virtio_mmio_guest_ptr(
		queue->desc, sizeof(struct vring_desc) * queue->num);
	avail = orlix_virtio_mmio_guest_ptr(queue->avail, sizeof(*avail));
	if (!desc || !avail)
		return;

	while (queue->last_avail != orlix_vring_read16(avail->idx)) {
		unsigned int head =
			orlix_vring_read16(avail->ring[queue->last_avail % queue->num]);
		unsigned int written = 0;

		orlix_virtio_mmio_process_console_output_desc(desc, head,
							      &written);
		orlix_virtio_mmio_push_used(slot, queue, head, written);
		queue->last_avail++;
	}
}

static void orlix_virtio_mmio_process_rng_desc(
	const struct vring_desc *desc,
	unsigned int head,
	unsigned int *written)
{
	unsigned int descriptor_index = head;
	unsigned int guard;

	if (head >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	for (guard = 0; guard < ORLIX_VIRTIO_MMIO_QUEUE_SIZE; guard++) {
		void *buffer;
		u32 length;
		u16 flags;
		u64 address;
		unsigned long copied;

		if (descriptor_index >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
			return;

		length = orlix_vring_read32(desc[descriptor_index].len);
		flags = orlix_vring_read16(desc[descriptor_index].flags);
		address = orlix_vring_read64(desc[descriptor_index].addr);

		if (!(flags & VRING_DESC_F_WRITE))
			return;

		buffer = orlix_virtio_mmio_guest_ptr(address, length);
		if (!buffer)
			return;

		copied = orlix_host_entropy_read(buffer, length);
		*written += copied;
		if (copied < length)
			return;

		if (!(flags & VRING_DESC_F_NEXT))
			return;

		descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
	}
}

static void orlix_virtio_mmio_process_rng_queue(
	struct orlix_virtio_mmio_slot *slot,
	u32 queue_index)
{
	struct orlix_virtio_mmio_queue *queue;
	struct vring_desc *desc;
	struct vring_avail *avail;

	if (queue_index != 0 || queue_index >= ARRAY_SIZE(slot->queues))
		return;

	queue = &slot->queues[queue_index];
	if (!queue->ready || !queue->num || queue->num > ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	desc = orlix_virtio_mmio_guest_ptr(
		queue->desc, sizeof(struct vring_desc) * queue->num);
	avail = orlix_virtio_mmio_guest_ptr(queue->avail, sizeof(*avail));
	if (!desc || !avail)
		return;

	while (queue->last_avail != orlix_vring_read16(avail->idx)) {
		unsigned int head =
			orlix_vring_read16(avail->ring[queue->last_avail % queue->num]);
		unsigned int written = 0;

		orlix_virtio_mmio_process_rng_desc(desc, head, &written);
		orlix_virtio_mmio_push_used(slot, queue, head, written);
		queue->last_avail++;
	}
}

static void orlix_virtio_mmio_fill_fs_root_attr(struct fuse_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->ino = FUSE_ROOT_ID;
	attr->mode = S_IFDIR | 0555;
	attr->nlink = 2;
	attr->blksize = 4096;
}

static void orlix_virtio_mmio_fill_fs_root_entry(
	struct fuse_entry_out *entry)
{
	memset(entry, 0, sizeof(*entry));
	entry->nodeid = FUSE_ROOT_ID;
	entry->entry_valid = 1;
	entry->attr_valid = 1;
	orlix_virtio_mmio_fill_fs_root_attr(&entry->attr);
}

static u64 orlix_virtio_mmio_fs_host_nodeid(unsigned int entry_index)
{
	return ORLIX_VIRTIO_MMIO_FS_HOST_NODE_BASE + entry_index;
}

static bool orlix_virtio_mmio_fs_host_index(u64 nodeid, unsigned int *entry_index)
{
	if (nodeid < ORLIX_VIRTIO_MMIO_FS_HOST_NODE_BASE)
		return false;

	*entry_index = nodeid - ORLIX_VIRTIO_MMIO_FS_HOST_NODE_BASE;
	return *entry_index < ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT &&
	       nodeid < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_BASE;
}

static u64 orlix_virtio_mmio_fs_child_nodeid(
	unsigned int parent_entry_index,
	unsigned int entry_index)
{
	return ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_BASE +
	       parent_entry_index * ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE +
	       entry_index;
}

static bool orlix_virtio_mmio_fs_child_index(
	u64 nodeid,
	unsigned int *parent_entry_index,
	unsigned int *entry_index)
{
	u64 child_offset;

	if (nodeid < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_BASE)
		return false;

	child_offset = nodeid - ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_BASE;
	*parent_entry_index = child_offset / ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE;
	*entry_index = child_offset % ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE;
	return *parent_entry_index < ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT &&
	       *entry_index < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE;
}

static u32 orlix_virtio_mmio_fs_host_type_mode(u8 type)
{
	switch (type) {
	case ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY:
		return S_IFDIR;
	case ORLIX_HOST_DIRECTORY_ENTRY_SYMLINK:
		return S_IFLNK;
	case ORLIX_HOST_DIRECTORY_ENTRY_REGULAR:
	default:
		return S_IFREG;
	}
}

static u32 orlix_virtio_mmio_fs_host_dirent_type(u8 type)
{
	switch (type) {
	case ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY:
		return DT_DIR;
	case ORLIX_HOST_DIRECTORY_ENTRY_SYMLINK:
		return DT_LNK;
	case ORLIX_HOST_DIRECTORY_ENTRY_REGULAR:
	default:
		return DT_REG;
	}
}

static bool orlix_virtio_mmio_fs_readonly_opcode(u32 opcode)
{
	switch (opcode) {
	case FUSE_SETATTR:
	case FUSE_SYMLINK:
	case FUSE_MKNOD:
	case FUSE_MKDIR:
	case FUSE_UNLINK:
	case FUSE_RMDIR:
	case FUSE_RENAME:
	case FUSE_LINK:
	case FUSE_WRITE:
	case FUSE_SETXATTR:
	case FUSE_REMOVEXATTR:
	case FUSE_CREATE:
	case FUSE_FALLOCATE:
		return true;
	default:
		return false;
	}
}

static bool orlix_virtio_mmio_fs_no_data_success_opcode(u32 opcode)
{
	switch (opcode) {
	case FUSE_RELEASE:
	case FUSE_FLUSH:
	case FUSE_FSYNC:
	case FUSE_RELEASEDIR:
	case FUSE_FSYNCDIR:
		return true;
	default:
		return false;
	}
}

static void orlix_virtio_mmio_fill_fs_host_attr(
	struct fuse_attr *attr,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	memset(attr, 0, sizeof(*attr));
	attr->ino = orlix_virtio_mmio_fs_host_nodeid(entry_index);
	attr->size = host_entry->size;
	attr->mode = orlix_virtio_mmio_fs_host_type_mode(host_entry->type) |
		     (host_entry->mode & 0777);
	attr->nlink = host_entry->type == ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY ? 2 : 1;
	attr->blksize = 4096;
}

static void orlix_virtio_mmio_fill_fs_host_entry(
	struct fuse_entry_out *entry,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	memset(entry, 0, sizeof(*entry));
	entry->nodeid = orlix_virtio_mmio_fs_host_nodeid(entry_index);
	entry->entry_valid = 1;
	entry->attr_valid = 1;
	orlix_virtio_mmio_fill_fs_host_attr(&entry->attr, entry_index, host_entry);
}

static void orlix_virtio_mmio_fill_fs_child_attr(
	struct fuse_attr *attr,
	unsigned int parent_entry_index,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	orlix_virtio_mmio_fill_fs_host_attr(attr, entry_index, host_entry);
	attr->ino = orlix_virtio_mmio_fs_child_nodeid(parent_entry_index,
						     entry_index);
}

static void orlix_virtio_mmio_fill_fs_child_entry(
	struct fuse_entry_out *entry,
	unsigned int parent_entry_index,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	memset(entry, 0, sizeof(*entry));
	entry->nodeid = orlix_virtio_mmio_fs_child_nodeid(parent_entry_index,
							 entry_index);
	entry->entry_valid = 1;
	entry->attr_valid = 1;
	orlix_virtio_mmio_fill_fs_child_attr(&entry->attr, parent_entry_index,
					     entry_index, host_entry);
}

static bool orlix_virtio_mmio_fuse_name_is(
	const struct fuse_in_header *in,
	u32 in_capacity,
	const char *expected)
{
	const char *name = (const void *)(in + 1);
	u32 expected_length = strlen(expected);
	u32 available;

	if (in_capacity <= sizeof(*in) || in->len <= sizeof(*in))
		return false;

	available = min_t(u32, in_capacity, in->len) - sizeof(*in);
	return available == expected_length &&
	       memcmp(name, expected, expected_length) == 0;
}

static bool orlix_virtio_mmio_fuse_name_matches(
	const struct fuse_in_header *in,
	u32 in_capacity,
	const char *expected)
{
	const char *name = (const void *)(in + 1);
	u32 available;
	u32 expected_length;

	if (in_capacity <= sizeof(*in) || in->len <= sizeof(*in))
		return false;

	available = min_t(u32, in_capacity, in->len) - sizeof(*in);
	expected_length = strnlen(expected, ORLIX_HOST_DIRECTORY_NAME_MAX + 1);
	return expected_length <= ORLIX_HOST_DIRECTORY_NAME_MAX &&
	       available == expected_length &&
	       memcmp(name, expected, expected_length) == 0;
}

static int orlix_virtio_mmio_find_fs_host_entry(
	const struct fuse_in_header *in,
	u32 in_capacity,
	unsigned int *entry_index,
	struct orlix_host_directory_entry *host_entry)
{
	unsigned int index;

	for (index = 0; index < ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT; index++) {
		if (orlix_host_directory_read_entry(
			    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY, index, host_entry) != 0)
			return -ENOENT;
		if (orlix_virtio_mmio_fuse_name_matches(in, in_capacity,
							host_entry->name)) {
			*entry_index = index;
			return 0;
		}
	}

	return -ENOENT;
}

static int orlix_virtio_mmio_find_fs_host_child_entry(
	const struct fuse_in_header *in,
	u32 in_capacity,
	unsigned int parent_entry_index,
	unsigned int *entry_index,
	struct orlix_host_directory_entry *host_entry)
{
	unsigned int index;

	for (index = 0; index < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE; index++) {
		if (orlix_host_directory_read_child_entry(
			    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
			    parent_entry_index, index, host_entry) != 0)
			return -ENOENT;
		if (orlix_virtio_mmio_fuse_name_matches(in, in_capacity,
							host_entry->name)) {
			*entry_index = index;
			return 0;
		}
	}

	return -ENOENT;
}

static bool orlix_virtio_mmio_append_fs_dirent(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	const char *name,
	u32 name_length,
	u64 next_offset)
{
	struct fuse_dirent *dirent;
	u32 record_length;

	if (*written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + name_length);
	if (record_length > out_capacity - *written)
		return false;

	dirent = (void *)((u8 *)out + *written);
	memset(dirent, 0, record_length);
	dirent->ino = FUSE_ROOT_ID;
	dirent->off = next_offset;
	dirent->namelen = name_length;
	dirent->type = DT_DIR;
	memcpy(dirent->name, name, name_length);

	*written += record_length;
	return true;
}

static bool orlix_virtio_mmio_append_fs_host_dirent(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	struct fuse_dirent *dirent;
	u32 name_length = strnlen(host_entry->name,
				  ORLIX_HOST_DIRECTORY_NAME_MAX + 1);
	u32 record_length;

	if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX ||
	    *written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + name_length);
	if (record_length > out_capacity - *written)
		return false;

	dirent = (void *)((u8 *)out + *written);
	memset(dirent, 0, record_length);
	dirent->ino = orlix_virtio_mmio_fs_host_nodeid(entry_index);
	dirent->off = entry_index + 3;
	dirent->namelen = name_length;
	dirent->type = orlix_virtio_mmio_fs_host_dirent_type(host_entry->type);
	memcpy(dirent->name, host_entry->name, name_length);

	*written += record_length;
	return true;
}

static bool orlix_virtio_mmio_append_fs_child_dirent(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	unsigned int parent_entry_index,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	struct fuse_dirent *dirent;
	u32 name_length = strnlen(host_entry->name,
				  ORLIX_HOST_DIRECTORY_NAME_MAX + 1);
	u32 record_length;

	if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX ||
	    *written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + name_length);
	if (record_length > out_capacity - *written)
		return false;

	dirent = (void *)((u8 *)out + *written);
	memset(dirent, 0, record_length);
	dirent->ino = orlix_virtio_mmio_fs_child_nodeid(parent_entry_index,
						       entry_index);
	dirent->off = entry_index + 1;
	dirent->namelen = name_length;
	dirent->type = orlix_virtio_mmio_fs_host_dirent_type(host_entry->type);
	memcpy(dirent->name, host_entry->name, name_length);

	*written += record_length;
	return true;
}

static bool orlix_virtio_mmio_append_fs_direntplus(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	const char *name,
	u32 name_length,
	u64 next_offset)
{
	struct fuse_direntplus *direntplus;
	u32 record_length;

	if (*written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(
		FUSE_NAME_OFFSET_DIRENTPLUS + name_length);
	if (record_length > out_capacity - *written)
		return false;

	direntplus = (void *)((u8 *)out + *written);
	memset(direntplus, 0, record_length);
	orlix_virtio_mmio_fill_fs_root_entry(&direntplus->entry_out);
	direntplus->dirent.ino = FUSE_ROOT_ID;
	direntplus->dirent.off = next_offset;
	direntplus->dirent.namelen = name_length;
	direntplus->dirent.type = DT_DIR;
	memcpy(direntplus->dirent.name, name, name_length);

	*written += record_length;
	return true;
}

static bool orlix_virtio_mmio_append_fs_host_direntplus(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	struct fuse_direntplus *direntplus;
	u32 name_length = strnlen(host_entry->name,
				  ORLIX_HOST_DIRECTORY_NAME_MAX + 1);
	u32 record_length;

	if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX ||
	    *written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(
		FUSE_NAME_OFFSET_DIRENTPLUS + name_length);
	if (record_length > out_capacity - *written)
		return false;

	direntplus = (void *)((u8 *)out + *written);
	memset(direntplus, 0, record_length);
	orlix_virtio_mmio_fill_fs_host_entry(&direntplus->entry_out,
					     entry_index, host_entry);
	direntplus->dirent.ino = orlix_virtio_mmio_fs_host_nodeid(entry_index);
	direntplus->dirent.off = entry_index + 3;
	direntplus->dirent.namelen = name_length;
	direntplus->dirent.type =
		orlix_virtio_mmio_fs_host_dirent_type(host_entry->type);
	memcpy(direntplus->dirent.name, host_entry->name, name_length);

	*written += record_length;
	return true;
}

static bool orlix_virtio_mmio_append_fs_child_direntplus(
	struct fuse_out_header *out,
	u32 out_capacity,
	unsigned int *written,
	unsigned int parent_entry_index,
	unsigned int entry_index,
	const struct orlix_host_directory_entry *host_entry)
{
	struct fuse_direntplus *direntplus;
	u32 name_length = strnlen(host_entry->name,
				  ORLIX_HOST_DIRECTORY_NAME_MAX + 1);
	u32 record_length;

	if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX ||
	    *written > out_capacity)
		return false;

	record_length = FUSE_DIRENT_ALIGN(
		FUSE_NAME_OFFSET_DIRENTPLUS + name_length);
	if (record_length > out_capacity - *written)
		return false;

	direntplus = (void *)((u8 *)out + *written);
	memset(direntplus, 0, record_length);
	orlix_virtio_mmio_fill_fs_child_entry(&direntplus->entry_out,
					      parent_entry_index, entry_index,
					      host_entry);
	direntplus->dirent.ino =
		orlix_virtio_mmio_fs_child_nodeid(parent_entry_index, entry_index);
	direntplus->dirent.off = entry_index + 1;
	direntplus->dirent.namelen = name_length;
	direntplus->dirent.type =
		orlix_virtio_mmio_fs_host_dirent_type(host_entry->type);
	memcpy(direntplus->dirent.name, host_entry->name, name_length);

	*written += record_length;
	return true;
}

static void orlix_virtio_mmio_process_fs_queue(
	struct orlix_virtio_mmio_slot *slot,
	u32 queue_index)
{
	struct orlix_virtio_mmio_queue *queue;
	struct vring_desc *desc;
	struct vring_avail *avail;

	if (queue_index == 0 || queue_index >= ARRAY_SIZE(slot->queues))
		return;

	queue = &slot->queues[queue_index];
	if (!queue->ready || !queue->num || queue->num > ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	desc = orlix_virtio_mmio_guest_ptr(
		queue->desc, sizeof(struct vring_desc) * queue->num);
	avail = orlix_virtio_mmio_guest_ptr(queue->avail, sizeof(*avail));
	if (!desc || !avail)
		return;

	while (queue->last_avail != orlix_vring_read16(avail->idx)) {
		struct fuse_in_header *in = NULL;
		struct fuse_out_header *out = NULL;
		u32 in_capacity = 0;
		u32 out_capacity = 0;
		unsigned int descriptor_index =
			orlix_vring_read16(avail->ring[queue->last_avail % queue->num]);
		unsigned int head = descriptor_index;
		unsigned int guard;

		for (guard = 0; guard < ORLIX_VIRTIO_MMIO_QUEUE_SIZE; guard++) {
			u32 length;
			u16 flags;
			u64 address;

			if (descriptor_index >= ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
				break;

			length = orlix_vring_read32(desc[descriptor_index].len);
			flags = orlix_vring_read16(desc[descriptor_index].flags);
			address = orlix_vring_read64(desc[descriptor_index].addr);

			if (flags & VRING_DESC_F_WRITE) {
				if (!out && length >= sizeof(*out)) {
					out = orlix_virtio_mmio_guest_ptr(address,
									 length);
					out_capacity = length;
				}
			} else if (!in && length >= sizeof(*in)) {
				in = orlix_virtio_mmio_guest_ptr(address, length);
				in_capacity = length;
			}

			if (!(flags & VRING_DESC_F_NEXT))
				break;
			descriptor_index = orlix_vring_read16(desc[descriptor_index].next);
		}

		if (in && !out &&
		    (in->opcode == FUSE_FORGET ||
		     in->opcode == FUSE_BATCH_FORGET)) {
			orlix_virtio_mmio_push_used(slot, queue, head, 0);
		} else if (in && out) {
			unsigned int written = sizeof(*out);

			out->error = -ENOSYS;
			out->unique = in->unique;

			if (in->opcode == FUSE_INIT &&
			    out_capacity >= sizeof(*out) + sizeof(struct fuse_init_out)) {
				struct fuse_init_out *init = (void *)(out + 1);

				memset(init, 0, sizeof(*init));
				init->major = FUSE_KERNEL_VERSION;
				init->minor = FUSE_KERNEL_MINOR_VERSION;
				init->max_write = 4096;
				init->time_gran = 1;
				init->max_background = 1;
				init->congestion_threshold = 1;
				init->max_pages = 1;
				out->error = 0;
				written += sizeof(*init);
			} else if (in->opcode == FUSE_DESTROY) {
				out->error = 0;
			} else if (in->opcode == FUSE_LOOKUP &&
				   in->nodeid == FUSE_ROOT_ID &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_entry_out)) {
				if (orlix_virtio_mmio_fuse_name_is(in, in_capacity, ".") ||
				    orlix_virtio_mmio_fuse_name_is(in, in_capacity, "..")) {
					struct fuse_entry_out *entry = (void *)(out + 1);

					orlix_virtio_mmio_fill_fs_root_entry(entry);
					out->error = 0;
					written += sizeof(*entry);
				} else {
					struct fuse_entry_out *entry = (void *)(out + 1);
					struct orlix_host_directory_entry host_entry;
					unsigned int entry_index;

					if (orlix_virtio_mmio_find_fs_host_entry(
						    in, in_capacity, &entry_index,
						    &host_entry) == 0) {
						orlix_virtio_mmio_fill_fs_host_entry(
							entry, entry_index, &host_entry);
						out->error = 0;
						written += sizeof(*entry);
					} else {
						out->error = -ENOENT;
					}
				}
			} else if (in->opcode == FUSE_LOOKUP &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_entry_out)) {
				struct fuse_entry_out *entry = (void *)(out + 1);
				struct orlix_host_directory_entry parent_entry;
				struct orlix_host_directory_entry host_entry;
				unsigned int parent_entry_index;
				unsigned int entry_index;

				if (orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &parent_entry_index) &&
				    orlix_host_directory_read_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    parent_entry_index, &parent_entry) == 0 &&
				    parent_entry.type == ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY &&
				    orlix_virtio_mmio_find_fs_host_child_entry(
					    in, in_capacity, parent_entry_index,
					    &entry_index, &host_entry) == 0) {
					orlix_virtio_mmio_fill_fs_child_entry(
						entry, parent_entry_index, entry_index,
						&host_entry);
					out->error = 0;
					written += sizeof(*entry);
				}
			} else if (in->opcode == FUSE_GETATTR &&
				   in->nodeid == FUSE_ROOT_ID &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_attr_out)) {
				struct fuse_attr_out *attr = (void *)(out + 1);

				memset(attr, 0, sizeof(*attr));
				orlix_virtio_mmio_fill_fs_root_attr(&attr->attr);
				out->error = 0;
				written += sizeof(*attr);
			} else if (in->opcode == FUSE_GETATTR &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_attr_out)) {
				struct fuse_attr_out *attr = (void *)(out + 1);
				struct orlix_host_directory_entry host_entry;
				unsigned int entry_index;

				if (orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &entry_index) &&
				    orlix_host_directory_read_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    entry_index, &host_entry) == 0) {
					memset(attr, 0, sizeof(*attr));
					orlix_virtio_mmio_fill_fs_host_attr(
						&attr->attr, entry_index, &host_entry);
					out->error = 0;
					written += sizeof(*attr);
				} else {
					unsigned int parent_entry_index;

					if (orlix_virtio_mmio_fs_child_index(
						    in->nodeid, &parent_entry_index,
						    &entry_index) &&
				    orlix_host_directory_read_child_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    parent_entry_index, entry_index,
					    &host_entry) == 0) {
						memset(attr, 0, sizeof(*attr));
						orlix_virtio_mmio_fill_fs_child_attr(
							&attr->attr, parent_entry_index,
							entry_index, &host_entry);
						out->error = 0;
						written += sizeof(*attr);
					}
				}
			} else if (in->opcode == FUSE_OPEN &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_open_out)) {
				struct fuse_open_out *open = (void *)(out + 1);
				struct orlix_host_directory_entry host_entry;
				unsigned int entry_index;

				if (orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &entry_index) &&
				    orlix_host_directory_read_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    entry_index, &host_entry) == 0 &&
				    host_entry.type == ORLIX_HOST_DIRECTORY_ENTRY_REGULAR) {
					memset(open, 0, sizeof(*open));
					open->fh = in->nodeid;
					out->error = 0;
					written += sizeof(*open);
				} else {
					unsigned int parent_entry_index;

					if (orlix_virtio_mmio_fs_child_index(
						    in->nodeid, &parent_entry_index,
						    &entry_index) &&
					    orlix_host_directory_read_child_entry(
						    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
						    parent_entry_index, entry_index,
						    &host_entry) == 0 &&
					    host_entry.type == ORLIX_HOST_DIRECTORY_ENTRY_REGULAR) {
						memset(open, 0, sizeof(*open));
						open->fh = in->nodeid;
						out->error = 0;
						written += sizeof(*open);
					}
				}
			} else if (in->opcode == FUSE_OPENDIR &&
				   in->nodeid == FUSE_ROOT_ID &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_open_out)) {
				struct fuse_open_out *open = (void *)(out + 1);

				memset(open, 0, sizeof(*open));
				open->fh = FUSE_ROOT_ID;
				out->error = 0;
				written += sizeof(*open);
			} else if (in->opcode == FUSE_ACCESS &&
				   in_capacity >= sizeof(*in) +
						  sizeof(struct fuse_access_in)) {
				struct fuse_access_in *access = (void *)(in + 1);
				unsigned int entry_index;

				if (access->mask & ORLIX_VIRTIO_MMIO_FUSE_ACCESS_W_OK)
					out->error = -EACCES;
				else if (in->nodeid == FUSE_ROOT_ID)
					out->error = 0;
				else if (orlix_virtio_mmio_fs_host_index(in->nodeid,
									 &entry_index)) {
					struct orlix_host_directory_entry host_entry;

					if (orlix_host_directory_read_entry(
						    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
						    entry_index, &host_entry) == 0)
						out->error = 0;
					else
						out->error = -ENOENT;
				} else {
					unsigned int parent_entry_index;

					if (orlix_virtio_mmio_fs_child_index(
						    in->nodeid, &parent_entry_index,
						    &entry_index)) {
						struct orlix_host_directory_entry host_entry;

						if (orlix_host_directory_read_child_entry(
							    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							    parent_entry_index, entry_index,
							    &host_entry) == 0)
							out->error = 0;
						else
							out->error = -ENOENT;
					}
				}
			} else if (in->opcode == FUSE_READ) {
				struct fuse_read_in *read = (void *)(in + 1);
				unsigned int entry_index;

				if (in_capacity >= sizeof(*in) + sizeof(*read) &&
				    out_capacity > sizeof(*out) &&
				    orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &entry_index)) {
					u32 read_capacity = out_capacity - sizeof(*out);
					u32 read_length = min_t(u32, read->size, read_capacity);
					void *read_buffer = (void *)(out + 1);
					long read_count;

					read_count = orlix_host_directory_read_file(
						ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
						entry_index, read->offset, read_buffer,
						read_length);
					if (read_count >= 0) {
						out->error = 0;
						written += read_count;
					} else {
						out->error = -EIO;
					}
				} else if (in_capacity >= sizeof(*in) + sizeof(*read) &&
					   out_capacity > sizeof(*out)) {
					unsigned int parent_entry_index;

					if (orlix_virtio_mmio_fs_child_index(
						    in->nodeid, &parent_entry_index,
						    &entry_index)) {
						u32 read_capacity =
							out_capacity - sizeof(*out);
						u32 read_length = min_t(u32, read->size,
									 read_capacity);
						void *read_buffer = (void *)(out + 1);
						long read_count;

						read_count = orlix_host_directory_read_child_file(
							ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							parent_entry_index, entry_index,
							read->offset, read_buffer, read_length);
						if (read_count >= 0) {
							out->error = 0;
							written += read_count;
						} else {
							out->error = -EIO;
						}
					}
				}
			} else if (in->opcode == FUSE_READLINK) {
				unsigned int entry_index;

				if (out_capacity > sizeof(*out) &&
				    orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &entry_index)) {
					u32 read_capacity = out_capacity - sizeof(*out);
					void *read_buffer = (void *)(out + 1);
					long read_count;

					read_count = orlix_host_directory_read_link(
						ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
						entry_index, read_buffer, read_capacity);
					if (read_count >= 0) {
						out->error = 0;
						written += read_count;
					} else {
						out->error = -EIO;
					}
				} else if (out_capacity > sizeof(*out)) {
					unsigned int parent_entry_index;

					if (orlix_virtio_mmio_fs_child_index(
						    in->nodeid, &parent_entry_index,
						    &entry_index)) {
						u32 read_capacity =
							out_capacity - sizeof(*out);
						void *read_buffer = (void *)(out + 1);
						long read_count;

						read_count = orlix_host_directory_read_child_link(
							ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							parent_entry_index, entry_index,
							read_buffer, read_capacity);
						if (read_count >= 0) {
							out->error = 0;
							written += read_count;
						} else {
							out->error = -EIO;
						}
					}
				}
			} else if (in->opcode == FUSE_READDIR &&
				   in->nodeid == FUSE_ROOT_ID) {
				struct fuse_read_in *read = (void *)(in + 1);

				if (in_capacity >= sizeof(*in) + sizeof(*read)) {
					unsigned int host_index;

					out->error = 0;
					if (read->offset == 0 &&
					    !orlix_virtio_mmio_append_fs_dirent(
						    out, out_capacity, &written, ".", 1, 1))
						goto fs_response_ready;
					if (read->offset <= 1)
						orlix_virtio_mmio_append_fs_dirent(
							out, out_capacity, &written, "..", 2, 2);
					host_index = read->offset <= 2 ? 0 : read->offset - 2;
					for (; host_index < ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT;
					     host_index++) {
						struct orlix_host_directory_entry host_entry;

						if (orlix_host_directory_read_entry(
							    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							    host_index, &host_entry) != 0)
							break;
						if (!orlix_virtio_mmio_append_fs_host_dirent(
							    out, out_capacity, &written,
							    host_index, &host_entry))
							break;
					}
				}
			} else if (in->opcode == FUSE_READDIR) {
				struct fuse_read_in *read = (void *)(in + 1);
				struct orlix_host_directory_entry parent_entry;
				unsigned int parent_entry_index;

				if (in_capacity >= sizeof(*in) + sizeof(*read) &&
				    orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &parent_entry_index) &&
				    orlix_host_directory_read_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    parent_entry_index, &parent_entry) == 0 &&
				    parent_entry.type == ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY) {
					unsigned int child_index = read->offset;

					out->error = 0;
					for (; child_index < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE;
					     child_index++) {
						struct orlix_host_directory_entry host_entry;

						if (orlix_host_directory_read_child_entry(
							    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							    parent_entry_index, child_index,
							    &host_entry) != 0)
							break;
						if (!orlix_virtio_mmio_append_fs_child_dirent(
							    out, out_capacity, &written,
							    parent_entry_index, child_index,
							    &host_entry))
							break;
					}
				}
			} else if (in->opcode == FUSE_READDIRPLUS &&
				   in->nodeid == FUSE_ROOT_ID) {
				struct fuse_read_in *read = (void *)(in + 1);

				if (in_capacity >= sizeof(*in) + sizeof(*read)) {
					unsigned int host_index;

					out->error = 0;
					if (read->offset == 0 &&
					    !orlix_virtio_mmio_append_fs_direntplus(
						    out, out_capacity, &written, ".", 1, 1))
						goto fs_response_ready;
					if (read->offset <= 1)
						orlix_virtio_mmio_append_fs_direntplus(
							out, out_capacity, &written, "..", 2, 2);
					host_index = read->offset <= 2 ? 0 : read->offset - 2;
					for (; host_index < ORLIX_VIRTIO_MMIO_FS_HOST_SCAN_LIMIT;
					     host_index++) {
						struct orlix_host_directory_entry host_entry;

						if (orlix_host_directory_read_entry(
							    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							    host_index, &host_entry) != 0)
							break;
						if (!orlix_virtio_mmio_append_fs_host_direntplus(
							    out, out_capacity, &written,
							    host_index, &host_entry))
							break;
					}
				}
			} else if (in->opcode == FUSE_READDIRPLUS) {
				struct fuse_read_in *read = (void *)(in + 1);
				struct orlix_host_directory_entry parent_entry;
				unsigned int parent_entry_index;

				if (in_capacity >= sizeof(*in) + sizeof(*read) &&
				    orlix_virtio_mmio_fs_host_index(in->nodeid,
								    &parent_entry_index) &&
				    orlix_host_directory_read_entry(
					    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
					    parent_entry_index, &parent_entry) == 0 &&
				    parent_entry.type == ORLIX_HOST_DIRECTORY_ENTRY_DIRECTORY) {
					unsigned int child_index = read->offset;

					out->error = 0;
					for (; child_index < ORLIX_VIRTIO_MMIO_FS_CHILD_NODE_STRIDE;
					     child_index++) {
						struct orlix_host_directory_entry host_entry;

						if (orlix_host_directory_read_child_entry(
							    ORLIX_VIRTIO_MMIO_FS_HOST_DIRECTORY,
							    parent_entry_index, child_index,
							    &host_entry) != 0)
							break;
						if (!orlix_virtio_mmio_append_fs_child_direntplus(
							    out, out_capacity, &written,
							    parent_entry_index, child_index,
							    &host_entry))
							break;
					}
				}
			} else if (orlix_virtio_mmio_fs_no_data_success_opcode(in->opcode)) {
				out->error = 0;
			} else if (orlix_virtio_mmio_fs_readonly_opcode(in->opcode)) {
				out->error = -EROFS;
			} else if (in->opcode == FUSE_STATFS &&
				   out_capacity >= sizeof(*out) +
						   sizeof(struct fuse_statfs_out)) {
				struct fuse_statfs_out *statfs = (void *)(out + 1);

				memset(statfs, 0, sizeof(*statfs));
				statfs->st.bsize = 4096;
				statfs->st.frsize = 4096;
				statfs->st.namelen = NAME_MAX;
				out->error = 0;
				written += sizeof(*statfs);
			}

fs_response_ready:
			out->len = written;
			orlix_virtio_mmio_push_used(slot, queue, head, written);
		}

		queue->last_avail++;
	}
}

static void orlix_virtio_mmio_process_queue(
	struct orlix_virtio_mmio_slot *slot,
	u32 queue_index)
{
	struct orlix_virtio_mmio_queue *queue;
	struct vring_desc *desc;
	struct vring_avail *avail;

	if (queue_index >= ARRAY_SIZE(slot->queues))
		return;

	if (slot->device_id == VIRTIO_ID_CONSOLE) {
		if (queue_index == 0) {
			orlix_virtio_mmio_process_console_input_queue(slot);
			orlix_virtio_mmio_start_console_input_timer(slot);
		} else if (queue_index == 1) {
			orlix_virtio_mmio_process_console_output_queue(slot,
								       queue_index);
		}
		return;
	}

	if (slot->device_id == VIRTIO_ID_RNG) {
		orlix_virtio_mmio_process_rng_queue(slot, queue_index);
		return;
	}

	if (slot->device_id == VIRTIO_ID_FS) {
		orlix_virtio_mmio_process_fs_queue(slot, queue_index);
		return;
	}

	if (slot->device_id != VIRTIO_ID_BLOCK)
		return;

	queue = &slot->queues[queue_index];
	if (!queue->ready || !queue->num || queue->num > ORLIX_VIRTIO_MMIO_QUEUE_SIZE)
		return;

	desc = orlix_virtio_mmio_guest_ptr(
		queue->desc, sizeof(struct vring_desc) * queue->num);
	avail = orlix_virtio_mmio_guest_ptr(queue->avail, sizeof(*avail));
	if (!desc || !avail)
		return;

	while (queue->last_avail != orlix_vring_read16(avail->idx)) {
		unsigned int head =
			orlix_vring_read16(avail->ring[queue->last_avail % queue->num]);
		unsigned int written = 0;
		u8 result;

		result = orlix_virtio_mmio_process_block_request(slot, desc,
								 head, queue->num,
								 &written);
		if (result != VIRTIO_BLK_S_OK)
			written = 1;

		orlix_virtio_mmio_push_used(slot, queue, head, written);
		queue->last_avail++;
	}
}

static void orlix_virtio_mmio_notify_work(struct work_struct *work)
{
	struct orlix_virtio_mmio_slot *slot =
		container_of(work, struct orlix_virtio_mmio_slot, notify_work);
	unsigned int queue_index;
	unsigned long pending;

	do {
		pending = xchg(&slot->pending_queues, 0);
		for (queue_index = 0;
		     queue_index < ORLIX_VIRTIO_MMIO_QUEUE_COUNT;
		     queue_index++) {
			if (pending & BIT(queue_index))
				orlix_virtio_mmio_process_queue(slot, queue_index);
		}
	} while (READ_ONCE(slot->pending_queues));
}

static void orlix_virtio_mmio_schedule_queue(
	struct orlix_virtio_mmio_slot *slot,
	u32 queue_index)
{
	if (queue_index >= ORLIX_VIRTIO_MMIO_QUEUE_COUNT)
		return;

	if (slot->device_id != VIRTIO_ID_BLOCK) {
		orlix_virtio_mmio_process_queue(slot, queue_index);
		return;
	}

	if (!slot->notify_work_initialized) {
		INIT_WORK(&slot->notify_work, orlix_virtio_mmio_notify_work);
		slot->notify_work_initialized = true;
	}

	set_bit(queue_index, &slot->pending_queues);
	schedule_work(&slot->notify_work);
}

bool orlix_virtio_mmio_read32(unsigned long physical_address, u32 *value)
{
	struct orlix_virtio_mmio_slot *slot;
	struct orlix_virtio_mmio_queue *queue;
	unsigned long offset;
	u64 features;

	slot = orlix_virtio_mmio_find_slot(physical_address, &offset);
	if (!slot)
		return false;

	queue = orlix_virtio_mmio_selected_queue(slot);

	if (offset >= VIRTIO_MMIO_CONFIG) {
		*value = orlix_virtio_mmio_config_read32(
			slot, offset - VIRTIO_MMIO_CONFIG);
		return true;
	}

	switch (offset) {
	case VIRTIO_MMIO_MAGIC_VALUE:
		*value = ORLIX_VIRTIO_MMIO_MAGIC;
		break;
	case VIRTIO_MMIO_VERSION:
		*value = 2;
		break;
	case VIRTIO_MMIO_DEVICE_ID:
		*value = orlix_virtio_mmio_device_present(slot) ?
			 slot->device_id : 0;
		break;
	case VIRTIO_MMIO_VENDOR_ID:
		*value = ORLIX_VIRTIO_MMIO_VENDOR;
		break;
	case VIRTIO_MMIO_DEVICE_FEATURES:
		features = orlix_virtio_mmio_device_features(slot);
		*value = slot->device_features_sel < 2 ?
			 (u32)(features >> (slot->device_features_sel * 32)) : 0;
		break;
	case VIRTIO_MMIO_QUEUE_NUM_MAX:
		*value = queue ? ORLIX_VIRTIO_MMIO_QUEUE_SIZE : 0;
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		*value = queue ? queue->num : 0;
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		*value = queue ? queue->ready : 0;
		break;
	case VIRTIO_MMIO_INTERRUPT_STATUS:
		*value = slot->interrupt_status;
		break;
	case VIRTIO_MMIO_STATUS:
		*value = slot->status;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		*value = queue ? (u32)queue->desc : 0;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		*value = queue ? (u32)(queue->desc >> 32) : 0;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		*value = queue ? (u32)queue->avail : 0;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		*value = queue ? (u32)(queue->avail >> 32) : 0;
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		*value = queue ? (u32)queue->used : 0;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		*value = queue ? (u32)(queue->used >> 32) : 0;
		break;
	case VIRTIO_MMIO_SHM_LEN_LOW:
	case VIRTIO_MMIO_SHM_LEN_HIGH:
		*value = ~0U;
		break;
	case VIRTIO_MMIO_SHM_BASE_LOW:
	case VIRTIO_MMIO_SHM_BASE_HIGH:
	case VIRTIO_MMIO_CONFIG_GENERATION:
	default:
		*value = 0;
		break;
	}

	return true;
}

bool orlix_virtio_mmio_write32(unsigned long physical_address, u32 value)
{
	struct orlix_virtio_mmio_slot *slot;
	struct orlix_virtio_mmio_queue *queue;
	unsigned long offset;

	slot = orlix_virtio_mmio_find_slot(physical_address, &offset);
	if (!slot)
		return false;

	queue = orlix_virtio_mmio_selected_queue(slot);

	switch (offset) {
	case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
		slot->device_features_sel = value;
		break;
	case VIRTIO_MMIO_DRIVER_FEATURES:
		if (slot->driver_features_sel < ARRAY_SIZE(slot->driver_features))
			slot->driver_features[slot->driver_features_sel] = value;
		break;
	case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
		slot->driver_features_sel = value;
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		slot->queue_sel = value;
		queue = orlix_virtio_mmio_selected_queue(slot);
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		if (queue)
			queue->num = value;
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		if (queue) {
			queue->ready = value ? 1 : 0;
			if (!queue->ready)
				queue->last_avail = 0;
			else if (slot->device_id == VIRTIO_ID_CONSOLE &&
				 slot->queue_sel == 0)
				orlix_virtio_mmio_start_console_input_timer(slot);
		}
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		orlix_virtio_mmio_schedule_queue(slot, value);
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		if (queue)
			queue->desc = (queue->desc & 0xffffffff00000000ULL) | value;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		if (queue)
			queue->desc = ((u64)value << 32) | (queue->desc & 0xffffffffULL);
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		if (queue)
			queue->avail = (queue->avail & 0xffffffff00000000ULL) | value;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		if (queue)
			queue->avail = ((u64)value << 32) | (queue->avail & 0xffffffffULL);
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		if (queue)
			queue->used = (queue->used & 0xffffffff00000000ULL) | value;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		if (queue)
			queue->used = ((u64)value << 32) | (queue->used & 0xffffffffULL);
		break;
	case VIRTIO_MMIO_SHM_SEL:
		slot->shm_sel = value;
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		slot->interrupt_status &= ~value;
		break;
	case VIRTIO_MMIO_STATUS:
		if (value == 0) {
			if (slot->notify_work_initialized)
				cancel_work_sync(&slot->notify_work);
			slot->pending_queues = 0;
			memset(slot->queues, 0, sizeof(slot->queues));
			memset(slot->driver_features, 0, sizeof(slot->driver_features));
			slot->interrupt_status = 0;
		}
		slot->status = value & 0xff;
		break;
	default:
		break;
	}

	return true;
}

bool orlix_virtio_mmio_read8(unsigned long physical_address, u8 *value)
{
	u32 word;

	if (!orlix_virtio_mmio_read32(physical_address & ~0x3UL, &word))
		return false;

	*value = (u8)(word >> ((physical_address & 0x3UL) * 8));
	return true;
}

bool orlix_virtio_mmio_read16(unsigned long physical_address, u16 *value)
{
	u32 word;

	if (!orlix_virtio_mmio_read32(physical_address & ~0x3UL, &word))
		return false;

	*value = (u16)(word >> ((physical_address & 0x2UL) * 8));
	return true;
}

bool orlix_virtio_mmio_write8(unsigned long physical_address, u8 value)
{
	u32 word;
	unsigned long aligned = physical_address & ~0x3UL;
	unsigned int shift = (physical_address & 0x3UL) * 8;

	if (!orlix_virtio_mmio_read32(aligned, &word))
		return false;

	word &= ~(0xffU << shift);
	word |= (u32)value << shift;
	return orlix_virtio_mmio_write32(aligned, word);
}

bool orlix_virtio_mmio_write16(unsigned long physical_address, u16 value)
{
	u32 word;
	unsigned long aligned = physical_address & ~0x3UL;
	unsigned int shift = (physical_address & 0x2UL) * 8;

	if (!orlix_virtio_mmio_read32(aligned, &word))
		return false;

	word &= ~(0xffffU << shift);
	word |= (u32)value << shift;
	return orlix_virtio_mmio_write32(aligned, word);
}
