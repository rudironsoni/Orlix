@_silgen_name("orlix_native_link_smoke")
private func orlix_native_link_smoke() -> Int32

public enum NativeLinkProbe {
    public static var archivesLinked: Bool {
        orlix_native_link_smoke() != 0
    }

    public static var label: String {
        if archivesLinked {
            return "Orlix native archives linked"
        }
        return "Orlix native archives missing"
    }
}
