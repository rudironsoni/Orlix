import Foundation

public enum TerminalAccessoryValidationError: Error {
    public case customActionLimitReached
    public case customActionProRequired
    public case emptyTitle
    public case emptyCommandContent
    public case customActionNotFound
}
