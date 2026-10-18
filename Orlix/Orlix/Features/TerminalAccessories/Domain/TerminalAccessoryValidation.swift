import Foundation

package enum TerminalAccessoryValidationError: Error {
    package case customActionLimitReached
    package case customActionProRequired
    package case emptyTitle
    package case emptyCommandContent
    package case customActionNotFound
}
