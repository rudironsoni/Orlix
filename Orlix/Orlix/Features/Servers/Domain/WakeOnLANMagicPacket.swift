import Foundation

nonisolated package enum WakeOnLANMagicPacket {
    static package let byteCount = 102

    static package func data(for macAddress: WakeOnLANMACAddress) -> Data {
        var bytes = [UInt8](repeating: 0xFF, count: 6)
        bytes.reserveCapacity(byteCount)
        for _ in 0..<16 {
            bytes.append(contentsOf: macAddress.bytes)
        }
        return Data(bytes)
    }
}
