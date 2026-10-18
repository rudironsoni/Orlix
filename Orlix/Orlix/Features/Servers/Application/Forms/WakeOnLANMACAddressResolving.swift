#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package protocol WakeOnLANMACAddressResolving: Sendable {
    package func resolveMACAddress(
        for server: Server,
        credentials: ServerCredentials
    ) async throws -> WakeOnLANMACAddress
}
