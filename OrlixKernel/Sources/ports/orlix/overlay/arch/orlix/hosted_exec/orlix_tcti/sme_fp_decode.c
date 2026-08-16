/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/array_size.h>
#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/errno.h>

#include "decode_aarch64.h"
#include "sme_fp_decode.h"

struct orlix_tcti_sme_fp_manifest_row {
	u16 ordinal;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, \
					   mask, pattern, predicate, offset, length) \
	{ ordinal, mask, pattern },
static const struct orlix_tcti_sme_fp_manifest_row orlix_tcti_sme_fp_manifest[] = {
#include "isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

bool orlix_tcti_sme_fp_source_ordinal(u16 ordinal)
{
	switch (ordinal) {
	case 1946U: case 1947U: case 1950U: case 1951U: case 1952U: case 1953U:
	case 1960U: case 1961U: case 1962U: case 1963U:
	case 1969U: case 1970U: case 1973U: case 1974U: case 1975U: case 1976U:
	case 1986U: case 1987U: case 1990U: case 1991U: case 1992U: case 1993U:
	case 2000U: case 2001U: case 2002U: case 2003U: case 2004U: case 2005U:
	case 2006U: case 2007U: case 2008U: case 2009U: case 2018U: case 2019U:
	case 2020U: case 2021U: case 2022U: case 2023U: case 2024U: case 2025U:
	case 2026U: case 2027U: case 2036U: case 2037U: case 2038U: case 2039U:
	case 2040U: case 2041U: case 2042U: case 2043U: case 2044U: case 2045U:
	case 2046U: case 2047U: case 2055U: case 2056U: case 2057U: case 2058U:
	case 2059U: case 2060U: case 2061U: case 2062U: case 2063U: case 2064U:
	case 2065U: case 2066U: case 2070U: case 2071U: case 2074U: case 2075U:
	case 2091U: case 2092U: case 2093U: case 2094U: case 2095U: case 2096U:
	case 2097U: case 2098U: case 2102U: case 2103U: case 2106U: case 2107U:
	case 2108U: case 2109U: case 2110U: case 2111U: case 2112U: case 2113U:
	case 2114U: case 2115U: case 2116U: case 2117U: case 2118U: case 2119U:
	case 2120U: case 2121U: case 2122U: case 2123U: case 2130U: case 2131U:
	case 2138U: case 2139U: case 2140U: case 2141U:
		return true;
	default:
		return false;
	}
}

int orlix_tcti_decode_sme_fp(u32 instruction,
			     struct orlix_tcti_decoded_instruction *decoded)
{
	size_t index;
	const struct orlix_tcti_sme_fp_manifest_row *selected = NULL;
	unsigned int selected_specificity = 0;

	if (!decoded)
		return -EINVAL;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_sme_fp_manifest); index++) {
		const struct orlix_tcti_sme_fp_manifest_row *row =
			&orlix_tcti_sme_fp_manifest[index];

		if (!orlix_tcti_sme_fp_source_ordinal(row->ordinal) ||
		    (instruction & row->mask) != row->pattern)
			continue;
		if (!selected || hweight32(row->mask) > selected_specificity) {
			selected = row;
			selected_specificity = hweight32(row->mask);
			continue;
		}
		if (hweight32(row->mask) == selected_specificity &&
		    row->ordinal != selected->ordinal)
			return -EINVAL;
	}
	if (selected) {
		decoded->decode_class = ORLIX_TCTI_DECODE_SME_FP;
		decoded->sme_fp_source_ordinal = selected->ordinal;
		return 0;
	}

	return -ENOENT;
}
