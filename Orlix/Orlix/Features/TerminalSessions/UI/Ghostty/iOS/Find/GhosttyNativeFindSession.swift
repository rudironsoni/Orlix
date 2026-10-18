#if os(iOS)
import UIKit

@available(iOS 16.0, *)
@MainActor
final package class GhosttyNativeFindSession: UIFindSession {
    package typealias SearchHandler = (_ query: String, _ options: UITextSearchOptions?) -> Void
    package typealias NavigateHandler = (_ direction: UITextStorageDirection) -> Void
    package typealias InvalidateHandler = () -> Void

    private let onSearch: SearchHandler
    private let onNavigate: NavigateHandler
    private let onInvalidate: InvalidateHandler

    private var reportedResultCount = 0
    private var reportedHighlightedResultIndex = NSNotFound
    private(set) var currentQuery = ""

    override package var resultCount: Int {
        reportedResultCount
    }

    override package var highlightedResultIndex: Int {
        reportedHighlightedResultIndex
    }

    override package var supportsReplacement: Bool {
        false
    }

    override package var allowsReplacementForCurrentlyHighlightedResult: Bool {
        false
    }

    package init(
        onSearch: @escaping SearchHandler,
        onNavigate: @escaping NavigateHandler,
        onInvalidate: @escaping InvalidateHandler
    ) {
        self.onSearch = onSearch
        self.onNavigate = onNavigate
        self.onInvalidate = onInvalidate
        super.init()
        searchResultDisplayStyle = .currentAndTotal
    }

    override package func performSearch(query: String, options: UITextSearchOptions?) {
        currentQuery = query
        resetReportedResults()
        onSearch(query, options)
    }

    override package func highlightNextResult(in direction: UITextStorageDirection) {
        onNavigate(direction)
    }

    override package func invalidateFoundResults() {
        currentQuery = ""
        resetReportedResults()
        onInvalidate()
    }

    package func applyExternalQuery(_ query: String) {
        currentQuery = query
    }

    package func updateReportedResults(total: Int?, highlightedIndex: Int?) -> Bool {
        let normalizedTotal = max(total ?? 0, 0)
        let normalizedHighlighted: Int
        if let highlightedIndex,
           normalizedTotal > 0,
           highlightedIndex >= 0,
           highlightedIndex < normalizedTotal {
            normalizedHighlighted = highlightedIndex
        } else {
            normalizedHighlighted = NSNotFound
        }

        guard reportedResultCount != normalizedTotal ||
                reportedHighlightedResultIndex != normalizedHighlighted else {
            return false
        }

        reportedResultCount = normalizedTotal
        reportedHighlightedResultIndex = normalizedHighlighted
        return true
    }

    package func resetReportedResults() {
        _ = updateReportedResults(total: 0, highlightedIndex: nil)
    }
}
#endif
