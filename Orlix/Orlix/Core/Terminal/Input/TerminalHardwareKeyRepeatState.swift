import Foundation

nonisolated package struct TerminalHardwareKeyRepeatState<Payload> {
    nonisolated package struct Active {
        package let token: UUID
        package let keyCode: UInt16
        package var payload: Payload
    }

    nonisolated package enum Phase {
        case idle
        case repeating(Active)
    }

    nonisolated package enum Registration {
        case started(Active)
        case updated(Active)
    }

    private(set) var phase: Phase = .idle

    @discardableResult
    mutating package func register(
        keyCode: UInt16,
        payload: Payload
    ) -> Registration {
        if case .repeating(var active) = phase,
           active.keyCode == keyCode {
            active.payload = payload
            phase = .repeating(active)
            return .updated(active)
        }

        let active = Active(
            token: UUID(),
            keyCode: keyCode,
            payload: payload
        )
        phase = .repeating(active)
        return .started(active)
    }

    package func active(for token: UUID) -> Active? {
        guard case .repeating(let active) = phase,
              active.token == token else {
            return nil
        }
        return active
    }

    @discardableResult
    mutating package func end(keyCode: UInt16) -> Active? {
        guard case .repeating(let active) = phase,
              active.keyCode == keyCode else {
            return nil
        }
        phase = .idle
        return active
    }

    @discardableResult
    mutating package func cancel() -> Active? {
        guard case .repeating(let active) = phase else {
            return nil
        }
        phase = .idle
        return active
    }
}
