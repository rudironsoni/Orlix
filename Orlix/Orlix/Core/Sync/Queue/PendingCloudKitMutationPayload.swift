import Foundation

nonisolated package enum PendingCloudKitMutationOperation: String, Codable, Equatable, Sendable {
    package case upsert
    package case delete
}

nonisolated package struct PendingCloudKitPayloadEnvelope: Codable, Equatable, Sendable {
    package let entityType: String
    package let entityKey: String
    package let operation: PendingCloudKitMutationOperation
    package let drainPriority: Int
    package let encodedValue: Data

    package init<Value: Encodable>(
        entityType: String,
        entityKey: String,
        operation: PendingCloudKitMutationOperation,
        drainPriority: Int,
        value: Value
    ) throws {
        self.entityType = entityType
        self.entityKey = entityKey
        self.operation = operation
        self.drainPriority = drainPriority
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        self.encodedValue = try encoder.encode(value)
    }

    package var coalescingKey: String {
        "\(entityType):\(entityKey)"
    }

    package var isDelete: Bool {
        operation == .delete
    }

    package var description: String {
        "\(entityType) \(operation.rawValue) \(entityKey)"
    }

    package func decode<Value: Decodable>(
        _ type: Value.Type,
        entityType expectedEntityType: String,
        operation expectedOperation: PendingCloudKitMutationOperation
    ) throws -> Value? {
        guard entityType == expectedEntityType,
              operation == expectedOperation else {
            return nil
        }
        return try JSONDecoder().decode(type, from: encodedValue)
    }

    package func validate(entityKey expectedEntityKey: String, drainPriority expectedPriority: Int) throws {
        guard entityKey == expectedEntityKey,
              drainPriority == expectedPriority else {
            throw PendingCloudKitPayloadEnvelopeError.invalidRoutingMetadata
        }
    }
}

nonisolated enum PendingCloudKitPayloadEnvelopeError: Error, Equatable {
    case invalidRoutingMetadata
}
