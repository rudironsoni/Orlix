import Foundation

actor AnalyticsProductionTransport {
    func send(_ event: TrackEventRequest) async {
        guard let eventType = OrlixAnalyticsEvent(rawValue: event.name) else { return }
        let properties = (event.data ?? [:]).mapValues { $0.telemetryValue }
        await OrlixTelemetry.shared.track(eventType, properties: properties)
    }
}

actor AnalyticsDeliveryQueue {
    typealias Delivery = @Sendable (TrackEventRequest) async -> Void

    static let defaultCapacity = 32

    private let capacity: Int
    private let delivery: Delivery
    private var pending: [TrackEventRequest] = []
    private var isDraining = false

    init(capacity: Int = defaultCapacity, delivery: @escaping Delivery) {
        self.capacity = max(1, capacity)
        self.delivery = delivery
    }

    @discardableResult
    func enqueue(_ event: TrackEventRequest) async -> Bool {
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

    func pendingCount() -> Int {
        pending.count
    }
}
