#include <stdbool.h>
#include <string.h>
#include <mach/machine.h>
#include <mach-o/loader.h>

/*
 * Xcode 26 Swift/compiler-rt import these libSystem symbols. They are iOS 16+
 * APIs. iOS 15.5 libSystem does not export them, so dyld aborts at launch.
 * Define them in the app so the binary does not import them from libSystem.
 */

const char *macho_arch_name_for_cpu_type(cpu_type_t type, cpu_subtype_t subtype)
{
	if (type == CPU_TYPE_ARM64) {
		if ((subtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E)
			return "arm64e";
		return "arm64";
	}
	if (type == CPU_TYPE_X86_64)
		return "x86_64";
	if (type == CPU_TYPE_ARM)
		return "arm";
	return NULL;
}

bool macho_cpu_type_for_arch_name(const char *archName, cpu_type_t *type, cpu_subtype_t *subtype)
{
	if (archName == NULL || type == NULL || subtype == NULL)
		return false;
	if (strcmp(archName, "arm64") == 0) {
		*type = CPU_TYPE_ARM64;
		*subtype = CPU_SUBTYPE_ARM64_ALL;
		return true;
	}
	if (strcmp(archName, "arm64e") == 0) {
		*type = CPU_TYPE_ARM64;
		*subtype = CPU_SUBTYPE_ARM64E;
		return true;
	}
	if (strcmp(archName, "x86_64") == 0) {
		*type = CPU_TYPE_X86_64;
		*subtype = CPU_SUBTYPE_X86_64_ALL;
		return true;
	}
	return false;
}

const char *macho_arch_name_for_mach_header(const struct mach_header *mh)
{
	if (mh == NULL)
		return "arm64";
	return macho_arch_name_for_cpu_type(mh->cputype, mh->cpusubtype);
}
