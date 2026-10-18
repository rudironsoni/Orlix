import Foundation

package actor AnalyticsProductionTransport {
    package func send(_ event: TrackEventRequest) async {
        guard let eventType = OrlixAnalyticsEvent(rawValue: event.name) else { return }
        let properties = (event.data ?? [:]).mapValues { $0.telemetryValue }
        await OrlixTelemetry.shared.track(eventType, properties: properties)
    }
}

package actor AnalyticsDeliveryQueue {
    package typealias Delivery = @Sendable (TrackEventRequest) async -> Void

    static package let defaultCapacity = 32

    private let capacity: Int
    private let delivery: Delivery
    private var pending: [TrackEventRequest] = []
    private var isDraining = false

    package init(capacity: Int = defaultCapacity, delivery: @escaping Delivery) {
        self.capacity = max(1, capacity)
        self.delivery = delivery
    }

    @discardableResult
    package func enqueue(_ event: TrackEventRequest) async -> Bool {
        guard pending.count < capacity else { return false }
        pending.append(event)
        guard !isDraining else { return true }

        isDraining = true
        while !pending.isEmpty {
            let next = pending.removeFirst()
            await delivery(next)
        }
        isDraining = false
        return true
    }

    package func pendingCount() -> Int {
        pending.count
    }
}
