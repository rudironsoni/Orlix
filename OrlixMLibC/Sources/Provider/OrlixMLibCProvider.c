// OrlixMLibC's meaningful output is the staged Linux libc sysroot built by
// OrlixMLibC/Makefile. This private framework target is its Xcode dependency
// boundary; it intentionally exposes no application-facing API.
__attribute__((visibility("hidden")))
const char *orlix_mlibc_package_identifier(void)
{
	return "com.rudironsoni.orlix.os.mlibc";
}
