import Foundation
import MoshCore

package enum MoshStartupReadiness {
    nonisolated static package func isTransportEstablished(by operations: [MoshHostOp]) -> Bool {
        // MoshClientSession publishes host operations only after decoding an
        // authenticated inbound UDP packet. Official mosh-server encodes a
        // quiet resize acknowledgement with a present zero-length diff, which
        // swift-mosh publishes as hostBytes(Data()).
        !operations.isEmpty
    }

    nonisolated static package func visibleTerminalBytes(from operation: MoshHostOp) -> Data? {
        guard case .hostBytes(let bytes) = operation, !bytes.isEmpty else { return nil }
        return bytes
    }
}
