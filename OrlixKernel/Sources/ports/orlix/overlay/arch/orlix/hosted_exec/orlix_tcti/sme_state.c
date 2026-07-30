/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/orlix_tcti.h>

void orlix_tcti_sme_state_release(struct orlix_tcti_sme_state *state)
{
	if (!state)
		return;
	kvfree(state->za);
	kvfree(state->zt0);
	memset(state, 0, sizeof(*state));
}

int orlix_tcti_sme_state_reset(struct orlix_tcti_sme_state *state,
				u16 svl_bytes, bool streaming_mode, bool za_enabled,
				bool zt0_valid)
{
	size_t za_bytes = 0;
	u8 *za = NULL;
	u8 *zt0 = NULL;

	if (!state || !svl_bytes || svl_bytes % ORLIX_TCTI_SVE_MIN_VL_BYTES)
		return -EINVAL;
	if (za_enabled && check_mul_overflow((size_t)svl_bytes,
					    (size_t)svl_bytes, &za_bytes))
		return -EOVERFLOW;
	if (za_enabled) {
		za = kvzalloc(za_bytes, GFP_KERNEL);
		if (!za)
			return -ENOMEM;
	}
	if (zt0_valid) {
		zt0 = kvzalloc(svl_bytes, GFP_KERNEL);
		if (!zt0) {
			kvfree(za);
			return -ENOMEM;
		}
	}
	orlix_tcti_sme_state_release(state);
	*state = (struct orlix_tcti_sme_state) {
		.svl_bytes = svl_bytes,
		.za = za,
		.zt0 = zt0,
		.za_bytes = za_bytes,
		.zt0_bytes = zt0_valid ? svl_bytes : 0U,
		.streaming_mode = streaming_mode,
		.za_enabled = za_enabled,
		.zt0_valid = zt0_valid,
		.valid = true,
	};
	return 0;
}

int orlix_tcti_sme_state_copy(struct orlix_tcti_sme_state *destination,
			       const struct orlix_tcti_sme_state *source)
{
	int ret;

	if (!destination || !source)
		return -EINVAL;
	if (!source->valid) {
		orlix_tcti_sme_state_release(destination);
		return 0;
	}
	if ((!source->za_enabled && source->za_bytes) ||
	    (source->za_enabled && (!source->za ||
		 source->za_bytes != (size_t)source->svl_bytes * source->svl_bytes)) ||
	    (!source->zt0_valid && source->zt0_bytes) ||
	    (source->zt0_valid && (!source->zt0 ||
		 source->zt0_bytes != source->svl_bytes)))
		return -EINVAL;
	ret = orlix_tcti_sme_state_reset(destination, source->svl_bytes,
		source->streaming_mode, source->za_enabled, source->zt0_valid);
	if (ret)
		return ret;
	if (source->za_bytes)
		memcpy(destination->za, source->za, source->za_bytes);
	if (source->zt0_bytes)
		memcpy(destination->zt0, source->zt0, source->zt0_bytes);
	return 0;
}
