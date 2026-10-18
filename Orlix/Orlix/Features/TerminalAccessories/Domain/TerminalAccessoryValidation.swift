import Foundation

package enum TerminalAccessoryValidationError: Error {
    case customActionLimitReached
    case customActionProRequired
    case emptyTitle
    case emptyCommandContent
    case customActionNotFound
}
