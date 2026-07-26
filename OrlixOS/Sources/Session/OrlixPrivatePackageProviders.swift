import Foundation

@_silgen_name("orlix_mlibc_package_identifier")
private func orlixMLibCPackageIdentifier() -> UnsafePointer<CChar>

@_silgen_name("orlix_coreutils_package_identifier")
private func orlixCoreUtilsPackageIdentifier() -> UnsafePointer<CChar>

enum OrlixPrivatePackageProviders {
    static func preconditionLinked() {
        precondition(
            String(cString: orlixMLibCPackageIdentifier()) ==
                "com.rudironsoni.orlix.os.mlibc"
        )
        precondition(
            String(cString: orlixCoreUtilsPackageIdentifier()) ==
                "com.rudironsoni.orlix.os.coreutils"
        )
    }
}
