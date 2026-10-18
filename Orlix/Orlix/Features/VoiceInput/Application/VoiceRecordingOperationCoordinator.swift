import Foundation
import Combine

@MainActor
final package class VoiceRecordingOperationCoordinator: ObservableObject {
    package enum Phase: Equatable {
        case idle
        case starting(operationID: UUID)
        case recording(operationID: UUID)
        case processing(operationID: UUID)

        package var operationID: UUID? {
            switch self {
            case .idle:
                return nil
            case .starting(let operationID),
                 .recording(let operationID),
                 .processing(let operationID):
                return operationID
            }
        }

        package var isActive: Bool { operationID != nil }

        package var isProcessing: Bool {
            if case .processing = self { return true }
            return false
        }
    }

    @Published private(set) var phase: Phase = .idle
    private var task: Task<Void, Never>?

    @discardableResult
    package func startRecording(
        operation: @escaping @MainActor (UUID) async throws -> Void,
        onStarted: @escaping @MainActor () -> Void = {},
        onFailure: @escaping @MainActor (Error) -> Void
    ) -> Task<Void, Never> {
        cancel()

        let operationID = UUID()
        phase = .starting(operationID: operationID)
        let task = Task { @MainActor [weak self] in
            do {
                try Task.checkCancellation()
                guard self?.phase == .starting(operationID: operationID) else { return }
                try await operation(operationID)
                try Task.checkCancellation()
                guard self?.finishStarting(operationID) == true else { return }
                onStarted()
            } catch is CancellationError {
                _ = self?.finish(operationID)
            } catch {
                guard self?.finish(operationID) == true else { return }
                onFailure(error)
            }
        }
        self.task = task
        return task
    }

    @discardableResult
    package func startProcessing<Value>(
        operation: @escaping @MainActor (UUID) async throws -> Value,
        onSuccess: @escaping @MainActor (Value) -> Void,
        onFailure: @escaping @MainActor (Error) -> Void
    ) -> Task<Void, Never>? {
        guard case .recording(let operationID) = phase else { return nil }

        phase = .processing(operationID: operationID)
        let task = Task { @MainActor [weak self] in
            do {
                try Task.checkCancellation()
                let value = try await operation(operationID)
                try Task.checkCancellation()
                guard self?.finish(operationID) == true else { return }
                onSuccess(value)
            } catch is CancellationError {
                _ = self?.finish(operationID)
            } catch {
                guard self?.finish(operationID) == true else { return }
                onFailure(error)
            }
        }
        self.task = task
        return task
    }

    package func cancel() {
        phase = .idle
        task?.cancel()
        task = nil
    }

    package var isActive: Bool { phase.isActive }
    package var isProcessing: Bool { phase.isProcessing }

    private func finishStarting(_ operationID: UUID) -> Bool {
        guard phase == .starting(operationID: operationID) else { return false }
        phase = .recording(operationID: operationID)
        task = nil
        return true
    }

    private func finish(_ operationID: UUID) -> Bool {
        guard phase.operationID == operationID else { return false }
        phase = .idle
        task = nil
        return true
    }

    deinit {
        task?.cancel()
    }
}
