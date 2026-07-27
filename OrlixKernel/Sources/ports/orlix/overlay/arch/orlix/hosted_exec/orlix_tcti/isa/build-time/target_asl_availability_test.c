/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	FILE *input;
	FILE *output;
	long length;
	char *source;
	char *artifact = NULL;
	long artifact_length;
	struct orlix_tcti_arm_xml_package package;

	if (argc != 4)
		return 2;
	if (orlix_tcti_arm_xml_package_validate(argv[2], argv[3], &package) !=
	    ORLIX_TCTI_ARM_XML_PACKAGE_OK)
		return 1;
	input = fopen(argv[1], "rb");
	if (!input || fseek(input, 0, SEEK_END) || (length = ftell(input)) < 0 ||
	    fseek(input, 0, SEEK_SET)) {
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	source = malloc((size_t)length + 1U);
	if (!source || fread(source, 1, (size_t)length, input) != (size_t)length ||
	    fclose(input)) {
		free(source);
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	output = tmpfile();
	if (!output || orlix_tcti_target_asl_availability_emit(source, (size_t)length,
						    &package, output) !=
						    ORLIX_TCTI_TARGET_ASL_AVAILABILITY_OK ||
	    fflush(output) || fseek(output, 0, SEEK_SET)) {
		free(source);
		if (output)
			fclose(output);
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	if (fseek(output, 0, SEEK_END) || (artifact_length = ftell(output)) < 0 ||
	    fseek(output, 0, SEEK_SET)) {
		free(source);
		fclose(output);
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	artifact = malloc((size_t)artifact_length + 1U);
	if (!artifact || fread(artifact, 1, (size_t)artifact_length, output) !=
		(size_t)artifact_length) {
		free(artifact);
		free(source);
		fclose(output);
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	artifact[artifact_length] = '\0';
	if (!strstr(artifact, "arm_a64_isa_xml_a_profile_2026_06") ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_SHARED_SHA256) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_NOTICE_SHA256) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_INDEX_SHA256) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_LICENSE_CLASS) ||
	    !strstr(artifact, "aarchmrs=vFATAp1-A/build=818/ref=2026-06_rel") ||
	    !strstr(artifact,
		"instructions_sha256=a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe") ||
	    !strstr(artifact, "compatibility=both_arm_2026_06_a_profile") ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_RELEASE_DIGEST) ||
	    !strstr(artifact, "target_xml_matched=4332") ||
	    !strstr(artifact, "target_xml_encoding_absent=18") ||
	    !strstr(artifact, "target_xml_semantics_complete=4332") ||
	    !strstr(artifact, "target_xml_decode_missing=0") ||
	    !strstr(artifact, "target_xml_operation_missing=0") ||
	    !strstr(artifact, "ORLIX_TCTI_A64_ASL_CORPUS_PRESENT") ||
	    !strstr(artifact, "ORLIX_TCTI_A64_ASL_HELPERS_AVAILABLE") ||
	    !strstr(artifact,
		"\"operations/ABS/operation\",") ||
	    !strstr(artifact,
		"\"28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8\", "
		"ORLIX_TCTI_A64_ASL_BODY_PLACEHOLDER") ||
	    !strstr(artifact,
		"\"operations/ABS/decode\",") ||
	    !strstr(artifact,
		"\"74234e98afe7498fb5daf1f36ac2d78acc339464f950703b8c019892f982b90b\", "
		"ORLIX_TCTI_A64_ASL_DECODE_NULL") ||
	    !strstr(artifact,
		"\"ABS_32_dp_1src\", ORLIX_TCTI_A64_ASL_XML_SEMANTICS_PRESENT, "
		"\"abs.xml\"") ||
	    !strstr(artifact,
		"\"SETGOEN_memset_go\", ORLIX_TCTI_A64_ASL_XML_ENCODING_ABSENT, "
		"\"\"") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_BODY_PRESENT") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_CORPUS_ABSENT") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_HELPERS_UNAVAILABLE")) {
		free(artifact);
		free(source);
		fclose(output);
		orlix_tcti_arm_xml_package_destroy(&package);
		return 1;
	}
	free(artifact);
	free(source);
	fclose(output);
	orlix_tcti_arm_xml_package_destroy(&package);
	puts("PASS official shared-ASL package provenance with explicit inline absences");
	return 0;
}
