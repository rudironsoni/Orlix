import CloudKit
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalThemeCloudKitRecordCodec {
    static package let recordType = "TerminalTheme"
    static package let recordKeys = ["name", "content", "updatedAt", "deletedAt"]

    static package func theme(from record: CKRecord) -> TerminalTheme? {
        guard
            let id = UUID(uuidString: record.recordID.recordName),
            let name = record["name"] as? String,
            let content = record["content"] as? String,
            let validatedName = try? TerminalThemeValidator.validateAndNormalizeThemeName(name)
        else {
            return nil
        }

        return TerminalTheme(
            id: id,
            name: validatedName,
            content: content,
            updatedAt: record["updatedAt"] as? Date ?? Date.distantPast,
            deletedAt: record["deletedAt"] as? Date
        )
    }

    static package func record(for theme: TerminalTheme, in zoneID: CKRecordZone.ID) -> CKRecord {
        let recordID = CKRecord.ID(
            recordName: theme.id.uuidString,
            zoneID: zoneID
        )
        let record = CKRecord(recordType: recordType, recordID: recordID)
        record["name"] = theme.name
        record["content"] = theme.content
        record["updatedAt"] = theme.updatedAt
        record["deletedAt"] = theme.deletedAt
        return record
    }
}

nonisolated package enum TerminalThemePreferenceCloudKitRecordCodec {
    static package let recordType = "TerminalThemePreference"
    static package let recordName = TerminalThemePreference.recordName
    static package let recordKeys = [
        "darkThemeName", "lightThemeName", "usePerAppearanceTheme", "updatedAt"
    ]

    static package func preference(from record: CKRecord) -> TerminalThemePreference? {
        guard
            let darkThemeName = record["darkThemeName"] as? String,
            let lightThemeName = record["lightThemeName"] as? String,
            let usePerAppearanceTheme = record["usePerAppearanceTheme"] as? Int,
            let validatedDarkThemeName = try? TerminalThemeValidator.validateAndNormalizeThemeName(
                darkThemeName
            ),
            let validatedLightThemeName = try? TerminalThemeValidator.validateAndNormalizeThemeName(
                lightThemeName
            )
        else {
            return nil
        }

        return TerminalThemePreference(
            darkThemeName: validatedDarkThemeName,
            lightThemeName: validatedLightThemeName,
            usePerAppearanceTheme: usePerAppearanceTheme != 0,
            updatedAt: record["updatedAt"] as? Date ?? Date.distantPast
        )
    }

    static package func recordID(in zoneID: CKRecordZone.ID) -> CKRecord.ID {
        CKRecord.ID(recordName: recordName, zoneID: zoneID)
    }

    static package func record(
        for preference: TerminalThemePreference,
        in zoneID: CKRecordZone.ID
    ) -> CKRecord {
        let recordID = CKRecord.ID(
            recordName: recordName,
            zoneID: zoneID
        )
        let record = CKRecord(recordType: recordType, recordID: recordID)
        record["darkThemeName"] = preference.darkThemeName
        record["lightThemeName"] = preference.lightThemeName
        record["usePerAppearanceTheme"] = preference.usePerAppearanceTheme ? 1 : 0
        record["updatedAt"] = preference.updatedAt
        return record
    }
}

package extension TerminalThemePreference {
    nonisolated static package let recordName = "terminal-theme-preference.v1"
}
