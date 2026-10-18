import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct WakeOnLANNetworkInterface: Equatable, Sendable {
    package let address: WakeOnLANIPv4Address
    package let netmask: WakeOnLANIPv4Address
    package let isUp: Bool
    package let isRunning: Bool
    package let supportsBroadcast: Bool
    package let isLoopback: Bool
}

nonisolated package protocol WakeOnLANInterfaceProviding: Sendable {
    func interfaces() throws -> [WakeOnLANNetworkInterface]
}

nonisolated package enum WakeOnLANBroadcastResolver {
    static package func destinations(
        for interfaces: [WakeOnLANNetworkInterface]
    ) -> [WakeOnLANIPv4Address] {
        let addresses = interfaces.compactMap { interface -> WakeOnLANIPv4Address? in
            guard interface.isUp,
                  interface.isRunning,
                  interface.supportsBroadcast,
                  !interface.isLoopback else {
                return nil
            }

            let address = interface.address.hostOrderValue
            let netmask = interface.netmask.hostOrderValue
            guard address != 0, netmask != 0, netmask != UInt32.max else {
                return nil
            }

            let hostMask = ~netmask
            guard hostMask & (hostMask &+ 1) == 0 else {
                return nil
            }

            return WakeOnLANIPv4Address(hostOrderValue: address | hostMask)
        }

        return Array(Set(addresses)).sorted {
            $0.hostOrderValue < $1.hostOrderValue
        }
    }
}
