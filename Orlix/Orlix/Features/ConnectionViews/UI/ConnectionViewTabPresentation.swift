#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package extension ConnectionViewTabID {
    package var localizedKey: String {
        switch self {
        case .stats: "Stats"
        case .terminal: "Terminal"
        case .files: "Files"
        }
    }

    package var icon: String {
        switch self {
        case .stats: "chart.bar.xaxis"
        case .terminal: "terminal"
        case .files: "folder"
        }
    }
}
