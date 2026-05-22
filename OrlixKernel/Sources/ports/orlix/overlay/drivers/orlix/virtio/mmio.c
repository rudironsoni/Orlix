// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/types.h>
#include <uapi/linux/virtio_mmio.h>
#include <internal/asm/virtio_mmio.h>

#define ORLIX_VIRTIO_MMIO_MAGIC ('v' | ('i' << 8) | ('r' << 16) | ('t' << 24))
#define ORLIX_VIRTIO_MMIO_VENDOR 0x4f524c58U
#define ORLIX_VIRTIO_MMIO_SLOT_SIZE 0x200UL

struct orlix_virtio_mmio_slot {
	unsigned long base;
	u32 status;
	u32 device_features_sel;
	u32 driver_features_sel;
	u32 driver_features[2];
	u32 queue_sel;
	u32 queue_num;
	u32 queue_ready;
	u32 queue_desc_low;
	u32 queue_desc_high;
	u32 queue_avail_low;
	u32 queue_avail_high;
	u32 queue_used_low;
	u32 queue_used_high;
	u32 shm_sel;
};

static struct orlix_virtio_mmio_slot orlix_virtio_mmio_slots[] = {
	{ .base = 0x10001000UL },
	{ .base = 0x10001200UL },
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

bool orlix_virtio_mmio_read32(unsigned long physical_address, u32 *value)
{
	struct orlix_virtio_mmio_slot *slot;
	unsigned long offset;

	slot = orlix_virtio_mmio_find_slot(physical_address, &offset);
	if (!slot)
		return false;

	switch (offset) {
	case VIRTIO_MMIO_MAGIC_VALUE:
		*value = ORLIX_VIRTIO_MMIO_MAGIC;
		break;
	case VIRTIO_MMIO_VERSION:
		*value = 2;
		break;
	case VIRTIO_MMIO_DEVICE_ID:
		*value = 0;
		break;
	case VIRTIO_MMIO_VENDOR_ID:
		*value = ORLIX_VIRTIO_MMIO_VENDOR;
		break;
	case VIRTIO_MMIO_DEVICE_FEATURES:
		*value = 0;
		break;
	case VIRTIO_MMIO_QUEUE_NUM_MAX:
		*value = 0;
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		*value = slot->queue_num;
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		*value = slot->queue_ready;
		break;
	case VIRTIO_MMIO_INTERRUPT_STATUS:
		*value = 0;
		break;
	case VIRTIO_MMIO_STATUS:
		*value = slot->status;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		*value = slot->queue_desc_low;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		*value = slot->queue_desc_high;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		*value = slot->queue_avail_low;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		*value = slot->queue_avail_high;
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		*value = slot->queue_used_low;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		*value = slot->queue_used_high;
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
	unsigned long offset;

	slot = orlix_virtio_mmio_find_slot(physical_address, &offset);
	if (!slot)
		return false;

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
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		slot->queue_num = value;
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		slot->queue_ready = value ? 1 : 0;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		slot->queue_desc_low = value;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		slot->queue_desc_high = value;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		slot->queue_avail_low = value;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		slot->queue_avail_high = value;
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		slot->queue_used_low = value;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		slot->queue_used_high = value;
		break;
	case VIRTIO_MMIO_SHM_SEL:
		slot->shm_sel = value;
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		break;
	case VIRTIO_MMIO_STATUS:
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
