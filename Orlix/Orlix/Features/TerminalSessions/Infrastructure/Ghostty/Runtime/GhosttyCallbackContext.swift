import Foundation

package extension Ghostty {
    /// Stable userdata for C callbacks without extending the owner's lifetime.
    nonisolated final package class CallbackContext<Owner: AnyObject>: @unchecked Sendable {
        private let lock = NSLock()
        private weak var owner: Owner?

        package init(owner: Owner) {
            self.owner = owner
        }

        package var userdata: UnsafeMutableRawPointer {
            Unmanaged.passUnretained(self).toOpaque()
        }

        package func resolve() -> Owner? {
            lock.withLock { owner }
        }

        package func invalidate() {
            lock.withLock {
                owner = nil
            }
        }

        static package func resolve(_ userdata: UnsafeMutableRawPointer?) -> Owner? {
            guard let userdata else { return nil }
            return Unmanaged<CallbackContext>
                .fromOpaque(userdata)
                .takeUnretainedValue()
                .resolve()
        }
    }
}
