import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct RemoteShellStartupActionFormModel: Equatable, Sendable {
    package enum ValidationError: Equatable, Sendable {
        case invalidCommand
        case commandTooLong
    }

    package var command: String

    package init(action: RemoteShellStartupAction?) {
        command = action?.command ?? ""
    }

    package var validationError: ValidationError? {
        do {
            _ = try makeAction()
            return nil
        } catch let error as RemoteShellStartupAction.ValidationError {
            return switch error {
            case .empty:
                nil
            case .containsUnsupportedControlCharacters:
                .invalidCommand
            case .tooLong:
                .commandTooLong
            }
        } catch {
            return .invalidCommand
        }
    }

    package var isValid: Bool {
        validationError == nil
    }

    package func makeAction() throws -> RemoteShellStartupAction? {
        guard !command.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return nil
        }
        return try RemoteShellStartupAction(command: command)
    }

    package func commandForPersistence() -> String? {
        do {
            return try makeAction()?.command
        } catch {
            return nil
        }
    }
}
