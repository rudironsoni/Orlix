// OrlixCoreUtils' meaningful output is its staged Linux userspace payload.
// This private framework target is an Xcode dependency boundary and does not
// provide an application-facing Coreutils facade.
__attribute__((visibility("hidden")))
const char *orlix_coreutils_package_identifier(void)
{
	return "com.rudironsoni.orlix.os.coreutils";
}
