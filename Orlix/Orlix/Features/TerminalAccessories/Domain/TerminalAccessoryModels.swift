import Foundation

nonisolated public enum TerminalSnippetSendMode: String, Codable, CaseIterable, Identifiable, Sendable {
    public case insert
    public case insertAndEnter

    public var id: String { rawValue }
}

nonisolated public enum TerminalAccessoryCustomActionKind: String, Codable, CaseIterable, Identifiable, Sendable {
    public case command
    public case shortcut

    public var id: String { rawValue }
}

nonisolated package struct TerminalAccessoryShortcutModifiers: Codable, Equatable, Hashable, Sendable {
    package var control: Bool = false
    package var alternate: Bool = false
    package var command: Bool = false
    package var shift: Bool = false

    private enum CodingKeys: String, CodingKey {
        case control
        case alternate
        case command
        case shift
    }

    static package let none = TerminalAccessoryShortcutModifiers()

    package init(
        control: Bool = false,
        alternate: Bool = false,
        command: Bool = false,
        shift: Bool = false
    ) {
        self.control = control
        self.alternate = alternate
        self.command = command
        self.shift = shift
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        control = try container.decodeIfPresent(Bool.self, forKey: .control) ?? false
        alternate = try container.decodeIfPresent(Bool.self, forKey: .alternate) ?? false
        command = try container.decodeIfPresent(Bool.self, forKey: .command) ?? false
        shift = try container.decodeIfPresent(Bool.self, forKey: .shift) ?? false
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(control, forKey: .control)
        try container.encode(alternate, forKey: .alternate)
        try container.encode(command, forKey: .command)
        try container.encode(shift, forKey: .shift)
    }

}

nonisolated public enum TerminalAccessoryShortcutKey: String, Codable, CaseIterable, Identifiable, Sendable {
    public case a
    public case b
    public case c
    public case d
    public case e
    public case f
    public case g
    public case h
    public case i
    public case j
    public case k
    public case l
    public case m
    public case n
    public case o
    public case p
    public case q
    public case r
    public case s
    public case t
    public case u
    public case v
    public case w
    public case x
    public case y
    public case z
    public case digit0
    public case digit1
    public case digit2
    public case digit3
    public case digit4
    public case digit5
    public case digit6
    public case digit7
    public case digit8
    public case digit9
    public case backquote
    public case minus
    public case equal
    public case bracketLeft
    public case bracketRight
    public case backslash
    public case semicolon
    public case quote
    public case comma
    public case period
    public case slash
    public case space
    public case escape
    public case tab
    public case enter
    public case backspace
    public case delete
    public case insert
    public case home
    public case end
    public case pageUp
    public case pageDown
    public case arrowUp
    public case arrowDown
    public case arrowLeft
    public case arrowRight
    public case f1
    public case f2
    public case f3
    public case f4
    public case f5
    public case f6
    public case f7
    public case f8
    public case f9
    public case f10
    public case f11
    public case f12

    public var id: String { rawValue }

    public var unshiftedText: String? {
        switch self {
        case .a: return "a"
        case .b: return "b"
        case .c: return "c"
        case .d: return "d"
        case .e: return "e"
        case .f: return "f"
        case .g: return "g"
        case .h: return "h"
        case .i: return "i"
        case .j: return "j"
        case .k: return "k"
        case .l: return "l"
        case .m: return "m"
        case .n: return "n"
        case .o: return "o"
        case .p: return "p"
        case .q: return "q"
        case .r: return "r"
        case .s: return "s"
        case .t: return "t"
        case .u: return "u"
        case .v: return "v"
        case .w: return "w"
        case .x: return "x"
        case .y: return "y"
        case .z: return "z"
        case .digit0: return "0"
        case .digit1: return "1"
        case .digit2: return "2"
        case .digit3: return "3"
        case .digit4: return "4"
        case .digit5: return "5"
        case .digit6: return "6"
        case .digit7: return "7"
        case .digit8: return "8"
        case .digit9: return "9"
        case .backquote: return "`"
        case .minus: return "-"
        case .equal: return "="
        case .bracketLeft: return "["
        case .bracketRight: return "]"
        case .backslash: return "\\"
        case .semicolon: return ";"
        case .quote: return "'"
        case .comma: return ","
        case .period: return "."
        case .slash: return "/"
        case .space: return " "
        default: return nil
        }
    }

    public var shiftedText: String? {
        switch self {
        case .a: return "A"
        case .b: return "B"
        case .c: return "C"
        case .d: return "D"
        case .e: return "E"
        case .f: return "F"
        case .g: return "G"
        case .h: return "H"
        case .i: return "I"
        case .j: return "J"
        case .k: return "K"
        case .l: return "L"
        case .m: return "M"
        case .n: return "N"
        case .o: return "O"
        case .p: return "P"
        case .q: return "Q"
        case .r: return "R"
        case .s: return "S"
        case .t: return "T"
        case .u: return "U"
        case .v: return "V"
        case .w: return "W"
        case .x: return "X"
        case .y: return "Y"
        case .z: return "Z"
        case .digit0: return ")"
        case .digit1: return "!"
        case .digit2: return "@"
        case .digit3: return "#"
        case .digit4: return "$"
        case .digit5: return "%"
        case .digit6: return "^"
        case .digit7: return "&"
        case .digit8: return "*"
        case .digit9: return "("
        case .backquote: return "~"
        case .minus: return "_"
        case .equal: return "+"
        case .bracketLeft: return "{"
        case .bracketRight: return "}"
        case .backslash: return "|"
        case .semicolon: return ":"
        case .quote: return "\""
        case .comma: return "<"
        case .period: return ">"
        case .slash: return "?"
        case .space: return " "
        default: return nil
        }
    }
}

nonisolated public enum TerminalAccessorySystemActionID: String, Codable, CaseIterable, Hashable, Identifiable, Sendable {
    public case commandModifier
    public case escape
    public case tab
    public case shiftTab
    public case enter
    public case backspace
    public case delete
    public case insert
    public case home
    public case end
    public case pageUp
    public case pageDown
    public case arrowUp
    public case arrowDown
    public case arrowLeft
    public case arrowRight
    public case f1
    public case f2
    public case f3
    public case f4
    public case f5
    public case f6
    public case f7
    public case f8
    public case f9
    public case f10
    public case f11
    public case f12
    public case ctrlC
    public case ctrlD
    public case ctrlZ
    public case ctrlL
    public case ctrlA
    public case ctrlE
    public case ctrlK
    public case ctrlU
    public case mouseCapture
    public case unknown

    public var id: String { rawValue }

    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let rawValue = try container.decode(String.self)
        self = Self(rawValue: rawValue) ?? .unknown
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }

    public var isRepeatable: Bool {
        switch self {
        case .arrowUp, .arrowDown, .arrowLeft, .arrowRight, .backspace, .home, .end, .pageUp, .pageDown:
            return true
        default:
            return false
        }
    }
}

nonisolated public enum TerminalAccessoryItemRef: Codable, Hashable, Sendable {
    public case system(TerminalAccessorySystemActionID)
    public case custom(UUID)

    private enum CodingKeys: String, CodingKey {
        case kind
        case systemID
        case customActionID
        case snippetID
    }

    private enum Kind: String, Codable {
        case system
        case custom
        case snippet
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let kind = try container.decode(Kind.self, forKey: .kind)

        switch kind {
        case .system:
            let id = try container.decode(TerminalAccessorySystemActionID.self, forKey: .systemID)
            self = .system(id)
        case .custom:
            let id = try container.decode(UUID.self, forKey: .customActionID)
            self = .custom(id)
        case .snippet:
            let id = try container.decode(UUID.self, forKey: .snippetID)
            self = .custom(id)
        }
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)

        switch self {
        case .system(let id):
            try container.encode(Kind.system, forKey: .kind)
            try container.encode(id, forKey: .systemID)
        case .custom(let id):
            try container.encode(Kind.custom, forKey: .kind)
            try container.encode(id, forKey: .customActionID)
        }
    }
}

nonisolated package struct TerminalAccessoryCustomAction: Identifiable, Codable, Equatable, Sendable {
    package let id: UUID
    package var title: String
    package var kind: TerminalAccessoryCustomActionKind
    package var commandContent: String
    package var commandSendMode: TerminalSnippetSendMode
    package var shortcutKey: TerminalAccessoryShortcutKey
    package var shortcutModifiers: TerminalAccessoryShortcutModifiers
    package var updatedAt: Date
    package var deletedAt: Date?

    package init(
        id: UUID = UUID(),
        title: String,
        kind: TerminalAccessoryCustomActionKind,
        commandContent: String = "",
        commandSendMode: TerminalSnippetSendMode = .insert,
        shortcutKey: TerminalAccessoryShortcutKey = .a,
        shortcutModifiers: TerminalAccessoryShortcutModifiers = .none,
        updatedAt: Date = Date(),
        deletedAt: Date? = nil
    ) {
        self.id = id
        self.title = title
        self.kind = kind
        self.commandContent = commandContent
        self.commandSendMode = commandSendMode
        self.shortcutKey = shortcutKey
        self.shortcutModifiers = shortcutModifiers
        self.updatedAt = updatedAt
        self.deletedAt = deletedAt
    }

    package var isDeleted: Bool {
        deletedAt != nil
    }

}

nonisolated struct TerminalSnippet: Identifiable, Codable, Equatable, Sendable {
    let id: UUID
    var title: String
    var content: String
    var sendMode: TerminalSnippetSendMode
    var updatedAt: Date
    var deletedAt: Date?

    init(
        id: UUID = UUID(),
        title: String,
        content: String,
        sendMode: TerminalSnippetSendMode,
        updatedAt: Date = Date(),
        deletedAt: Date? = nil
    ) {
        self.id = id
        self.title = title
        self.content = content
        self.sendMode = sendMode
        self.updatedAt = updatedAt
        self.deletedAt = deletedAt
    }

    var isDeleted: Bool {
        deletedAt != nil
    }
}

nonisolated package struct TerminalAccessoryLayout: Codable, Equatable, Sendable {
    package var version: Int
    package var activeItems: [TerminalAccessoryItemRef]
    package var updatedAt: Date
}

nonisolated package struct TerminalAccessoryProfile: Codable, Equatable, Sendable {
    package var schemaVersion: Int
    package var layout: TerminalAccessoryLayout
    package var customActions: [TerminalAccessoryCustomAction]
    package var updatedAt: Date
    package var lastWriterDeviceId: String

    private enum CodingKeys: String, CodingKey {
        case schemaVersion
        case layout
        case customActions
        case snippets
        case updatedAt
        case lastWriterDeviceId
    }

    package init(
        schemaVersion: Int,
        layout: TerminalAccessoryLayout,
        customActions: [TerminalAccessoryCustomAction],
        updatedAt: Date,
        lastWriterDeviceId: String
    ) {
        self.schemaVersion = schemaVersion
        self.layout = layout
        self.customActions = customActions
        self.updatedAt = updatedAt
        self.lastWriterDeviceId = lastWriterDeviceId
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        schemaVersion = try container.decodeIfPresent(Int.self, forKey: .schemaVersion) ?? 1
        layout = try container.decodeIfPresent(TerminalAccessoryLayout.self, forKey: .layout)
            ?? TerminalAccessoryLayout(version: 1, activeItems: Self.defaultActiveItems, updatedAt: .distantPast)
        updatedAt = try container.decodeIfPresent(Date.self, forKey: .updatedAt) ?? .distantPast
        lastWriterDeviceId = try container.decodeIfPresent(String.self, forKey: .lastWriterDeviceId) ?? ""

        if let actions = try container.decodeIfPresent([TerminalAccessoryCustomAction].self, forKey: .customActions) {
            customActions = actions
        } else {
            let legacySnippets = try container.decodeIfPresent([TerminalSnippet].self, forKey: .snippets) ?? []
            customActions = legacySnippets.map(\.asCustomAction)
        }
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(schemaVersion, forKey: .schemaVersion)
        try container.encode(layout, forKey: .layout)
        try container.encode(customActions, forKey: .customActions)
        try container.encode(updatedAt, forKey: .updatedAt)
        try container.encode(lastWriterDeviceId, forKey: .lastWriterDeviceId)
    }
}

nonisolated package extension TerminalAccessoryProfile {
    static package let schemaVersion = 3
    static package let recordType = "UserPreference"
    static package let recordName = "terminalAccessory.v1"

    static package let minActiveItems = 4
    static package let maxActiveItems = 28
    static package let maxCustomActions = 100
    static package let maxCustomActionTitleLength = 24
    static package let maxCommandContentLength = 2048

    static package let legacyDefaultActiveItemsV2: [TerminalAccessoryItemRef] = [
        .system(.escape),
        .system(.tab),
        .system(.arrowUp),
        .system(.arrowDown),
        .system(.arrowLeft),
        .system(.arrowRight),
        .system(.backspace),
        .system(.ctrlC),
        .system(.ctrlD),
        .system(.ctrlZ),
        .system(.ctrlL),
        .system(.home),
        .system(.end),
        .system(.pageUp),
        .system(.pageDown)
    ]

    static package let defaultActiveItems: [TerminalAccessoryItemRef] = [
        .system(.escape),
        .system(.tab),
        .system(.mouseCapture),
        .system(.arrowUp),
        .system(.arrowDown),
        .system(.arrowLeft),
        .system(.arrowRight),
        .system(.backspace),
        .system(.ctrlC),
        .system(.ctrlD),
        .system(.ctrlZ),
        .system(.ctrlL),
        .system(.home),
        .system(.end),
        .system(.pageUp),
        .system(.pageDown)
    ]

    static package func defaultValue(lastWriterDeviceId: String) -> TerminalAccessoryProfile {
        TerminalAccessoryProfile(
            schemaVersion: schemaVersion,
            layout: TerminalAccessoryLayout(
                version: 1,
                activeItems: defaultActiveItems,
                updatedAt: .distantPast
            ),
            customActions: [],
            updatedAt: .distantPast,
            lastWriterDeviceId: lastWriterDeviceId
        )
    }

    static package var availableSystemActions: [TerminalAccessorySystemActionID] {
        TerminalAccessorySystemActionID.allCases.filter { $0 != .unknown }
    }

    package func normalized() -> TerminalAccessoryProfile {
        let shouldMigrateUntouchedDefaultLayout = schemaVersion < Self.schemaVersion
            && layout.activeItems == Self.legacyDefaultActiveItemsV2
        var customActionsByID: [UUID: TerminalAccessoryCustomAction] = [:]
        for action in customActions {
            let normalizedAction = action.normalized()
            if let existing = customActionsByID[normalizedAction.id] {
                if normalizedAction.updatedAt > existing.updatedAt {
                    customActionsByID[normalizedAction.id] = normalizedAction
                }
            } else {
                customActionsByID[normalizedAction.id] = normalizedAction
            }
        }

        let normalizedActions = customActionsByID.values.sorted { lhs, rhs in
            if lhs.updatedAt == rhs.updatedAt {
                return lhs.id.uuidString < rhs.id.uuidString
            }
            return lhs.updatedAt > rhs.updatedAt
        }

        let limitedActiveActionIDs = Set(
            normalizedActions
                .filter { !$0.isDeleted }
                .prefix(Self.maxCustomActions)
                .map(\.id)
        )

        let normalizedAndLimitedActions = normalizedActions.filter {
            $0.isDeleted || limitedActiveActionIDs.contains($0.id)
        }

        let activeActionIDs = Set(normalizedAndLimitedActions.filter { !$0.isDeleted }.map(\.id))

        var seenItems = Set<TerminalAccessoryItemRef>()
        var normalizedItems: [TerminalAccessoryItemRef] = []

        let activeItems = shouldMigrateUntouchedDefaultLayout
            ? Self.defaultActiveItems
            : layout.activeItems
        for item in activeItems {
            switch item {
            case .system(let actionID):
                guard actionID != .unknown else { continue }
            case .custom(let actionID):
                guard activeActionIDs.contains(actionID) else { continue }
            }

            guard !seenItems.contains(item) else { continue }
            seenItems.insert(item)
            normalizedItems.append(item)
        }

        if normalizedItems.count > Self.maxActiveItems {
            normalizedItems = Array(normalizedItems.prefix(Self.maxActiveItems))
        }

        if normalizedItems.count < Self.minActiveItems {
            normalizedItems = Self.defaultActiveItems
        }

        return TerminalAccessoryProfile(
            schemaVersion: max(Self.schemaVersion, schemaVersion),
            layout: TerminalAccessoryLayout(
                version: max(1, layout.version),
                activeItems: normalizedItems,
                updatedAt: layout.updatedAt
            ),
            customActions: Array(normalizedAndLimitedActions),
            updatedAt: updatedAt,
            lastWriterDeviceId: lastWriterDeviceId
        )
    }

    static package func merged(local: TerminalAccessoryProfile, remote: TerminalAccessoryProfile) -> TerminalAccessoryProfile {
        let normalizedLocal = local.normalized()
        let normalizedRemote = remote.normalized()

        let mergedLayout: TerminalAccessoryLayout
        if normalizedLocal.layout.updatedAt >= normalizedRemote.layout.updatedAt {
            mergedLayout = normalizedLocal.layout
        } else {
            mergedLayout = normalizedRemote.layout
        }

        var actionsByID: [UUID: TerminalAccessoryCustomAction] = [:]
        for action in normalizedRemote.customActions {
            actionsByID[action.id] = action
        }

        for action in normalizedLocal.customActions {
            if let existing = actionsByID[action.id] {
                if action.updatedAt >= existing.updatedAt {
                    actionsByID[action.id] = action
                }
            } else {
                actionsByID[action.id] = action
            }
        }

        let mergedActions = actionsByID.values.sorted { lhs, rhs in
            if lhs.updatedAt == rhs.updatedAt {
                return lhs.id.uuidString < rhs.id.uuidString
            }
            return lhs.updatedAt > rhs.updatedAt
        }

        let mergedUpdatedAt = max(
            normalizedLocal.updatedAt,
            normalizedRemote.updatedAt,
            mergedLayout.updatedAt,
            mergedActions.first?.updatedAt ?? .distantPast
        )

        let writerDeviceID: String
        if mergedUpdatedAt == normalizedLocal.updatedAt {
            writerDeviceID = normalizedLocal.lastWriterDeviceId
        } else if mergedUpdatedAt == normalizedRemote.updatedAt {
            writerDeviceID = normalizedRemote.lastWriterDeviceId
        } else if mergedLayout.updatedAt == normalizedLocal.layout.updatedAt {
            writerDeviceID = normalizedLocal.lastWriterDeviceId
        } else {
            writerDeviceID = normalizedRemote.lastWriterDeviceId
        }

        return TerminalAccessoryProfile(
            schemaVersion: max(normalizedLocal.schemaVersion, normalizedRemote.schemaVersion, Self.schemaVersion),
            layout: mergedLayout,
            customActions: Array(mergedActions),
            updatedAt: mergedUpdatedAt,
            lastWriterDeviceId: writerDeviceID
        )
        .normalized()
    }
}

private nonisolated extension TerminalAccessoryCustomAction {
    package func normalized() -> TerminalAccessoryCustomAction {
        let sanitizedTitle: String
        let sanitizedCommandContent: String

        if isDeleted {
            sanitizedTitle = ""
            sanitizedCommandContent = ""
        } else {
            let trimmedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
            sanitizedTitle = String(trimmedTitle.prefix(TerminalAccessoryProfile.maxCustomActionTitleLength))
            if kind == .command {
                sanitizedCommandContent = String(commandContent.prefix(TerminalAccessoryProfile.maxCommandContentLength))
            } else {
                sanitizedCommandContent = ""
            }
        }

        return TerminalAccessoryCustomAction(
            id: id,
            title: sanitizedTitle,
            kind: kind,
            commandContent: sanitizedCommandContent,
            commandSendMode: commandSendMode,
            shortcutKey: shortcutKey,
            shortcutModifiers: shortcutModifiers,
            updatedAt: updatedAt,
            deletedAt: deletedAt
        )
    }
}

private nonisolated extension TerminalSnippet {
    var asCustomAction: TerminalAccessoryCustomAction {
        TerminalAccessoryCustomAction(
            id: id,
            title: title,
            kind: .command,
            commandContent: content,
            commandSendMode: sendMode,
            shortcutKey: .a,
            shortcutModifiers: .none,
            updatedAt: updatedAt,
            deletedAt: deletedAt
        )
    }
}
