import Dispatch
import Foundation

nonisolated struct Duration: Sendable, Hashable, Comparable {
    nonisolated struct Components: Sendable, Hashable {
        let seconds: Int64
        let attoseconds: Int64
    }

    fileprivate let nanoseconds: Int64

    static let zero = Duration(nanoseconds: 0)

    static func seconds(_ value: Int) -> Duration {
        seconds(Int64(value))
    }

    static func seconds(_ value: Int64) -> Duration {
        Duration(nanoseconds: multipliedClamping(value, by: 1_000_000_000))
    }

    static func seconds(_ value: Double) -> Duration {
        fromFloatingPoint(value, scale: 1_000_000_000)
    }

    static func milliseconds(_ value: Int) -> Duration {
        milliseconds(Int64(value))
    }

    static func milliseconds(_ value: Int64) -> Duration {
        Duration(nanoseconds: multipliedClamping(value, by: 1_000_000))
    }

    static func milliseconds(_ value: Double) -> Duration {
        fromFloatingPoint(value, scale: 1_000_000)
    }

    var components: Components {
        let seconds = nanoseconds / 1_000_000_000
        let remainingNanoseconds = nanoseconds % 1_000_000_000
        return Components(
            seconds: seconds,
            attoseconds: remainingNanoseconds * 1_000_000_000
        )
    }

    static func < (lhs: Duration, rhs: Duration) -> Bool {
        lhs.nanoseconds < rhs.nanoseconds
    }

    static func * (lhs: Duration, rhs: Int) -> Duration {
        Duration(nanoseconds: multipliedClamping(lhs.nanoseconds, by: Int64(rhs)))
    }

    private static func fromFloatingPoint(_ value: Double, scale: Double) -> Duration {
        guard value.isFinite else {
            return Duration(nanoseconds: value.sign == .minus ? .min : .max)
        }
        let scaled = value * scale
        guard scaled < Double(Int64.max) else { return Duration(nanoseconds: .max) }
        guard scaled > Double(Int64.min) else { return Duration(nanoseconds: .min) }
        return Duration(nanoseconds: Int64(scaled.rounded()))
    }

    private static func multipliedClamping(_ lhs: Int64, by rhs: Int64) -> Int64 {
        let result = lhs.multipliedReportingOverflow(by: rhs)
        guard result.overflow else { return result.partialValue }
        return (lhs < 0) == (rhs < 0) ? .max : .min
    }
}

nonisolated struct ContinuousClock: Sendable {
    nonisolated struct Instant: Sendable, Hashable, Comparable {
        fileprivate let uptimeNanoseconds: UInt64

        static var now: Instant {
            Instant(uptimeNanoseconds: DispatchTime.now().uptimeNanoseconds)
        }

        func advanced(by duration: Duration) -> Instant {
            if duration.nanoseconds >= 0 {
                let delta = UInt64(duration.nanoseconds)
                let value = uptimeNanoseconds.addingReportingOverflow(delta)
                return Instant(uptimeNanoseconds: value.overflow ? .max : value.partialValue)
            }
            let delta = duration.nanoseconds == .min
                ? UInt64(Int64.max) + 1
                : UInt64(-duration.nanoseconds)
            return Instant(uptimeNanoseconds: uptimeNanoseconds > delta ? uptimeNanoseconds - delta : 0)
        }

        func duration(to other: Instant) -> Duration {
            if other.uptimeNanoseconds >= uptimeNanoseconds {
                let delta = other.uptimeNanoseconds - uptimeNanoseconds
                return Duration(nanoseconds: delta > UInt64(Int64.max) ? .max : Int64(delta))
            }
            let delta = uptimeNanoseconds - other.uptimeNanoseconds
            return Duration(nanoseconds: delta > UInt64(Int64.max) ? .min : -Int64(delta))
        }

        static func < (lhs: Instant, rhs: Instant) -> Bool {
            lhs.uptimeNanoseconds < rhs.uptimeNanoseconds
        }
    }

    static var now: Instant {
        .now
    }
}

extension Task where Success == Never, Failure == Never {
    static func sleep(for duration: Duration) async throws {
        guard duration.nanoseconds > 0 else {
            try Task.checkCancellation()
            return
        }
        try await Task.sleep(nanoseconds: UInt64(duration.nanoseconds))
    }
}
