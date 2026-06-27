import XCTest
import zlib
@_spi(OrlixPrivateTesting) @testable import OrlixOS

final class OrlixTerminalSessionTests: XCTestCase {
    func testBootStatusMapsAlreadyStartedResult() {
        XCTAssertEqual(OrlixBootStatus(rawStatus: -3), .alreadyStarted)
        XCTAssertEqual(
            OrlixBootStatus(rawStatus: -3).message,
            "Orlix boot already started in this process."
        )
    }

    func testDefaultEnvironmentDescriptorUsesLinuxShapedDefaults() {
        let descriptor = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.bundle.rootfs"
        )

        XCTAssertEqual(descriptor.id, "default")
        XCTAssertEqual(descriptor.source, .defaultRoot)
        XCTAssertEqual(descriptor.platform, "linux/arm64")
        XCTAssertEqual(descriptor.rootImageIdentifier, "orlix.bundle.rootfs")
        XCTAssertEqual(descriptor.defaultCommand, ["/bin/sh"])
        XCTAssertEqual(descriptor.defaultEnvironment["HOME"], "/root")
        XCTAssertEqual(
            descriptor.defaultEnvironment["PATH"],
            "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
        )
        XCTAssertEqual(descriptor.defaultWorkingDirectory, "/")
        XCTAssertEqual(descriptor.defaultUserID, 0)
        XCTAssertEqual(descriptor.defaultGroupID, 0)
        XCTAssertEqual(descriptor.rootMount, .defaultOverlay)
        XCTAssertEqual(descriptor.mounts, [])
        XCTAssertEqual(descriptor.rootMount.baseDevicePath, "/dev/vda")
        XCTAssertEqual(descriptor.rootMount.stateDevicePath, "/dev/vdb")
        XCTAssertEqual(descriptor.rootMount.lowerMountPath, "/lower")
        XCTAssertEqual(descriptor.rootMount.stateMountPath, "/state")
        XCTAssertEqual(descriptor.rootMount.overlayMountPath, "/newroot")
        XCTAssertEqual(descriptor.rootMount.upperDirectoryPath, "/state/upper")
        XCTAssertEqual(descriptor.rootMount.workDirectoryPath, "/state/work")
        XCTAssertEqual(descriptor.rootMount.filesystemType, "ext4")
        XCTAssertEqual(descriptor.rootMount.overlayFilesystemType, "overlay")
        XCTAssertTrue(descriptor.rootMount.baseReadOnly)
    }

    func testDocumentsMountIsExplicitEnvironmentDescriptorMetadata() throws {
        let documentsMount = try OrlixEnvironmentMount.documents(
            targetPath: "/home/root/Documents"
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "documents-explicit",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.documents-explicit",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [documentsMount]
        )

        XCTAssertEqual(descriptor.mounts, [documentsMount])
        XCTAssertEqual(descriptor.mounts.first?.source, .documents)
        XCTAssertEqual(descriptor.mounts.first?.targetPath, "/home/root/Documents")
        XCTAssertFalse(descriptor.mounts.first?.readOnly ?? true)

        let encoded = try JSONEncoder().encode(descriptor)
        let decoded = try JSONDecoder().decode(
            OrlixEnvironmentDescriptor.self,
            from: encoded
        )
        XCTAssertEqual(decoded, descriptor)
    }

    func testSecurityScopedExternalMountIsExplicitEnvironmentDescriptorMetadata()
        throws
    {
        let externalMount = try OrlixEnvironmentMount.securityScopedExternal(
            bookmarkID: "project-folder-bookmark",
            targetPath: "/mnt/external/project",
            readOnly: true
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "external-explicit",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.external-explicit",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [externalMount]
        )

        XCTAssertEqual(descriptor.mounts, [externalMount])
        XCTAssertEqual(
            descriptor.mounts.first?.source,
            .securityScopedExternal(bookmarkID: "project-folder-bookmark")
        )
        XCTAssertEqual(
            descriptor.mounts.first?.targetPath,
            "/mnt/external/project"
        )
        XCTAssertTrue(descriptor.mounts.first?.readOnly ?? false)

        let encoded = try JSONEncoder().encode(descriptor)
	let decoded = try JSONDecoder().decode(
		OrlixEnvironmentDescriptor.self,
		from: encoded
	)
	XCTAssertEqual(decoded, descriptor)
}

func testHostPathMountIsExplicitEnvironmentDescriptorMetadata() throws {
	let root = temporaryRegistryRoot()
	let hostDirectory = root.appendingPathComponent("host-bind", isDirectory: true)
	try FileManager.default.createDirectory(
		at: hostDirectory,
		withIntermediateDirectories: true
	)
	let hostMount = try OrlixEnvironmentMount.hostPath(
		hostDirectory.path,
		targetPath: "/mnt/host-bind",
		readOnly: true
	)
	let descriptor = OrlixEnvironmentDescriptor(
		id: "host-path-explicit",
		source: .copiedEnvironment(parentID: "default"),
		platform: "linux/arm64",
		rootImageIdentifier: "orlix.env.host-path-explicit",
		defaultCommand: ["/bin/sh"],
		defaultEnvironment: ["PATH": "/usr/bin:/bin"],
		defaultWorkingDirectory: "/",
		defaultUserID: 0,
		defaultGroupID: 0,
		mounts: [hostMount]
	)

	XCTAssertEqual(descriptor.mounts, [hostMount])
	XCTAssertEqual(descriptor.mounts.first?.source, .hostPath(hostDirectory.path))
	XCTAssertEqual(descriptor.mounts.first?.targetPath, "/mnt/host-bind")
	XCTAssertTrue(descriptor.mounts.first?.readOnly ?? false)

	let encoded = try JSONEncoder().encode(descriptor)
	let decoded = try JSONDecoder().decode(
		OrlixEnvironmentDescriptor.self,
		from: encoded
	)
	XCTAssertEqual(decoded, descriptor)
}

func testDocumentsMountRejectsReservedLinuxRuntimeTargets() throws {
	for target in ["/", "/proc", "/proc/self", "/dev", "/sys", "/run", "/tmp"] {
            XCTAssertThrowsError(
                try OrlixEnvironmentMount.documents(targetPath: target)
            ) { error in
                XCTAssertEqual(
                    error as? OrlixEnvironmentMountError,
                    .reservedTargetPath(target)
                )
            }
        }
    }

    func testSecurityScopedExternalMountRejectsUnsafeSourceIdentifiers()
        throws
    {
        for bookmarkID in ["", ".", "..", ".hidden", "a..b", "a/b", #"a\b"#, "a\u{0}b"] {
            XCTAssertThrowsError(
                try OrlixEnvironmentMount.securityScopedExternal(
                    bookmarkID: bookmarkID,
                    targetPath: "/mnt/external/project"
                )
            ) { error in
                XCTAssertEqual(
                    error as? OrlixEnvironmentMountError,
                    .invalidSourceIdentifier(bookmarkID)
                )
            }
        }
    }

    func testSecurityScopedExternalMountRejectsReservedLinuxRuntimeTargets()
        throws
    {
        for target in ["/", "/proc", "/proc/self", "/dev", "/sys", "/run", "/tmp"] {
            XCTAssertThrowsError(
                try OrlixEnvironmentMount.securityScopedExternal(
                    bookmarkID: "project-folder-bookmark",
                    targetPath: target
                )
            ) { error in
                XCTAssertEqual(
                    error as? OrlixEnvironmentMountError,
                    .reservedTargetPath(target)
                )
            }
        }
    }

    func testEnvironmentDescriptorRejectsPersistedInvalidDocumentsMountTargets()
        throws
    {
        for target in ["Documents", "/tmp", "/proc/self", "/home//root"] {
            let metadata = Data(
                """
                {
                  "defaultCommand" : [
                    "/bin/sh"
                  ],
                  "defaultEnvironment" : {
                    "PATH" : "/usr/bin:/bin"
                  },
                  "defaultGroupID" : 0,
                  "defaultUserID" : 0,
                  "defaultWorkingDirectory" : "/",
                  "id" : "bad-documents-mount",
                  "mounts" : [
                    {
                      "readOnly" : false,
                      "source" : {
                        "documents" : {}
                      },
                      "targetPath" : "\(target)"
                    }
                  ],
                  "platform" : "linux/arm64",
                  "rootImageIdentifier" : "orlix.env.bad-documents-mount",
                  "source" : {
                    "copiedEnvironment" : {
                      "parentID" : "default"
                    }
                  }
                }
                """.utf8
            )

            XCTAssertThrowsError(
                try JSONDecoder().decode(
                    OrlixEnvironmentDescriptor.self,
                    from: metadata
                )
            ) { error in
                switch target {
                case "/tmp", "/proc/self":
                    XCTAssertEqual(
                        error as? OrlixEnvironmentMountError,
                        .reservedTargetPath(target)
                    )
                default:
                    XCTAssertEqual(
                        error as? OrlixEnvironmentMountError,
                        .invalidTargetPath(target)
                    )
                }
            }
        }
    }

    func testEnvironmentStorageLayoutSeparatesStateScratchAndDownloads() throws {
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "alpine-dev"
        )

        XCTAssertTrue(
            layout.rootDirectory.path.hasSuffix(
                "Application Support/Orlix/environments/alpine-dev"
            )
        )
        XCTAssertTrue(
            layout.baseImageURL.path.hasSuffix(
                "Application Support/Orlix/environments/alpine-dev/base.ext4"
            )
        )
        XCTAssertTrue(
            layout.stateImageURL.path.hasSuffix(
                "Application Support/Orlix/environments/alpine-dev/state.ext4"
            )
        )
        XCTAssertTrue(
            layout.importScratchDirectory.path.hasSuffix(
                "Orlix/imports/alpine-dev"
            )
        )
        XCTAssertTrue(
            layout.downloadCacheDirectory.path.hasSuffix(
                "Caches/Orlix/downloads"
            )
        )
        XCTAssertFalse(layout.importScratchDirectory.path.contains("Application Support"))
        XCTAssertFalse(layout.downloadCacheDirectory.path.contains("Application Support"))
    }

	func testEnvironmentStorageLayoutRejectsUnsafeEnvironmentIDs() {
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: "")
		)
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.validateEnvironmentID("")
		)
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: "..")
		)
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.validateEnvironmentID("..")
		)
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: ".hidden")
		)
		XCTAssertNoThrow(
			try OrlixEnvironmentStorageLayout.validateEnvironmentID("alpine-dev")
		)
		XCTAssertThrowsError(
			try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: "a..b")
		)
        XCTAssertThrowsError(
            try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: "a/b")
        )
        XCTAssertThrowsError(
            try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: #"a\b"#)
        )
        XCTAssertThrowsError(
            try OrlixEnvironmentStorageLayout.layout(forEnvironmentID: "a\u{0}b")
        )
    }

    func testEnvironmentRegistryPersistsDescriptorsUnderStateRoot() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let documentsMount = try OrlixEnvironmentMount.documents(
            targetPath: "/home/root/Documents"
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-dev",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-dev",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [documentsMount]
        )

        try registry.save(descriptor)

        let loaded = try registry.load(environmentID: "alpine-dev")
        let listed = try registry.list()
        let layout = try registry.layout(forEnvironmentID: "alpine-dev")

        XCTAssertEqual(loaded, descriptor)
        XCTAssertEqual(listed, [descriptor])
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: layout.rootDirectory.path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: layout.importScratchDirectory.path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: layout.downloadCacheDirectory.path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: try registry.descriptorURL(
                    forEnvironmentID: "alpine-dev"
                ).path
            )
        )
        let metadata = try Data(
            contentsOf: try registry.descriptorURL(forEnvironmentID: "alpine-dev")
        )
        let decodedMetadata = try JSONSerialization.jsonObject(
            with: metadata
        ) as? [String: Any]
        let rootMount = try XCTUnwrap(decodedMetadata?["rootMount"] as? [String: Any])
        XCTAssertEqual(rootMount["baseDevicePath"] as? String, "/dev/vda")
        XCTAssertEqual(rootMount["stateDevicePath"] as? String, "/dev/vdb")
        XCTAssertEqual(rootMount["overlayMountPath"] as? String, "/newroot")
        let mounts = try XCTUnwrap(decodedMetadata?["mounts"] as? [[String: Any]])
        XCTAssertEqual(mounts.count, 1)
        XCTAssertEqual(mounts.first?["targetPath"] as? String, "/home/root/Documents")
        XCTAssertEqual(mounts.first?["readOnly"] as? Bool, false)
    }

    func testEnvironmentDescriptorDecodesLegacyMetadataWithDefaultOverlayRootMount()
        throws
    {
        let legacyMetadata = Data(
            """
            {
              "defaultCommand" : [
                "/bin/sh"
              ],
              "defaultEnvironment" : {
                "PATH" : "/usr/bin:/bin"
              },
              "defaultGroupID" : 0,
              "defaultUserID" : 0,
              "defaultWorkingDirectory" : "/",
              "id" : "legacy-alpine",
              "platform" : "linux/arm64",
              "rootImageIdentifier" : "orlix.env.legacy-alpine",
              "source" : {
                "rootfsTar" : {}
              }
            }
            """.utf8
        )

        let descriptor = try JSONDecoder().decode(
            OrlixEnvironmentDescriptor.self,
            from: legacyMetadata
        )

        XCTAssertEqual(descriptor.id, "legacy-alpine")
        XCTAssertEqual(descriptor.source, .rootfsTar)
        XCTAssertEqual(descriptor.rootMount, .defaultOverlay)
        XCTAssertEqual(descriptor.mounts, [])
    }

    func testEnvironmentRegistryDoesNotCreateBlockImagesWhenSavingMetadata() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.bundle.rootfs"
        )
        try registry.save(descriptor)

        let layout = try registry.layout(forEnvironmentID: descriptor.id)
        XCTAssertFalse(
            FileManager.default.fileExists(atPath: layout.baseImageURL.path)
        )
        XCTAssertFalse(
            FileManager.default.fileExists(atPath: layout.stateImageURL.path)
        )
    }

    func testEnvironmentRegistryCopiesNamedEnvironmentImagesAndMetadata()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let documentsMount = try OrlixEnvironmentMount.documents(
            targetPath: "/home/root/Documents",
            readOnly: true
        )
        let parent = OrlixEnvironmentDescriptor(
            id: "default",
            source: .defaultRoot,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.default",
            defaultCommand: ["/bin/sh", "-l"],
			defaultEnvironment: ["HOME": "/root", "PATH": "/usr/bin:/bin"],
			defaultWorkingDirectory: "/root",
			defaultUserID: 0,
			defaultGroupID: 0,
			defaultTerminal: false,
			mounts: [documentsMount]
        )
        try registry.save(parent)
        let parentLayout = try registry.layout(forEnvironmentID: parent.id)
        try Data("parent-base-image".utf8).write(to: parentLayout.baseImageURL)
        try Data("parent-state-image".utf8).write(to: parentLayout.stateImageURL)

        let copied = try registry.copyEnvironment(
            from: "default",
            to: "default-copy",
            rootImageIdentifier: "orlix.env.default-copy"
        )
        let copiedLayout = try registry.layout(forEnvironmentID: "default-copy")
        let loaded = try registry.load(environmentID: "default-copy")

        XCTAssertEqual(copied, loaded)
        XCTAssertEqual(copied.id, "default-copy")
        XCTAssertEqual(copied.source, .copiedEnvironment(parentID: "default"))
        XCTAssertEqual(copied.platform, parent.platform)
        XCTAssertEqual(copied.rootImageIdentifier, "orlix.env.default-copy")
        XCTAssertEqual(copied.defaultCommand, parent.defaultCommand)
        XCTAssertEqual(copied.defaultEnvironment, parent.defaultEnvironment)
	XCTAssertEqual(copied.defaultWorkingDirectory, parent.defaultWorkingDirectory)
	XCTAssertEqual(copied.defaultUserID, parent.defaultUserID)
	XCTAssertEqual(copied.defaultGroupID, parent.defaultGroupID)
	XCTAssertEqual(copied.defaultTerminal, parent.defaultTerminal)
	XCTAssertEqual(copied.rootMount, parent.rootMount)
        XCTAssertEqual(copied.cgroupUnified, parent.cgroupUnified)
        XCTAssertEqual(copied.deviceNodes, parent.deviceNodes)
        XCTAssertEqual(copied.mounts, parent.mounts)
        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.baseImageURL),
            try Data(contentsOf: parentLayout.baseImageURL)
        )
        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.stateImageURL),
            try Data(contentsOf: parentLayout.stateImageURL)
        )
        XCTAssertTrue(
            copiedLayout.baseImageURL.path.hasSuffix(
                "Application Support/Orlix/environments/default-copy/base.ext4"
            )
        )
        XCTAssertTrue(
            copiedLayout.importScratchDirectory.path.hasSuffix(
                "tmp/Orlix/imports/default-copy"
            )
        )
    }

    func testEnvironmentRegistryCopyRequiresMaterializedParentImages() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let parent = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.default"
        )
        try registry.save(parent)
        let parentLayout = try registry.layout(forEnvironmentID: parent.id)

        XCTAssertThrowsError(
            try registry.copyEnvironment(
                from: parent.id,
                to: "default-copy",
                rootImageIdentifier: "orlix.env.default-copy"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentCopyError,
                .missingParentImage(parentLayout.baseImageURL.path)
            )
        }

        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: try registry.layout(
                    forEnvironmentID: "default-copy"
                ).rootDirectory.path
            )
        )
    }

    func testEnvironmentRegistryCopyDoesNotOverwriteExistingEnvironment()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let parent = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.default"
        )
        try registry.save(parent)
        let parentLayout = try registry.layout(forEnvironmentID: parent.id)
        try Data("base".utf8).write(to: parentLayout.baseImageURL)
        try Data("state".utf8).write(to: parentLayout.stateImageURL)
        let existing = OrlixEnvironmentDescriptor(
            id: "default-copy",
            source: .copiedEnvironment(parentID: parent.id),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.existing",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: [:],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        try registry.save(existing)

        XCTAssertThrowsError(
            try registry.copyEnvironment(
                from: parent.id,
                to: existing.id,
                rootImageIdentifier: "orlix.env.default-copy"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentCopyError,
                .destinationExists(existing.id)
            )
        }

        XCTAssertEqual(
            try registry.load(environmentID: existing.id).rootImageIdentifier,
            "orlix.env.existing"
        )
    }

    func testEnvironmentRegistryCopyCreatesIndependentImageFiles() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let parent = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.default"
        )
        try registry.save(parent)
        let parentLayout = try registry.layout(forEnvironmentID: parent.id)
        try Data("parent-base-image".utf8).write(to: parentLayout.baseImageURL)
        try Data("parent-state-image".utf8).write(to: parentLayout.stateImageURL)

        _ = try registry.copyEnvironment(
            from: parent.id,
            to: "default-copy",
            rootImageIdentifier: "orlix.env.default-copy"
        )
        let copiedLayout = try registry.layout(forEnvironmentID: "default-copy")

        try Data("copied-base-image".utf8).write(to: copiedLayout.baseImageURL)
        try Data("copied-state-image".utf8).write(to: copiedLayout.stateImageURL)

        XCTAssertEqual(
            try Data(contentsOf: parentLayout.baseImageURL),
            Data("parent-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: parentLayout.stateImageURL),
            Data("parent-state-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.baseImageURL),
            Data("copied-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.stateImageURL),
            Data("copied-state-image".utf8)
        )
    }

    func testEnvironmentRegistryCopyPreservesStateAfterParentImageChanges()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let parent = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.default"
        )
        try registry.save(parent)
        let parentLayout = try registry.layout(forEnvironmentID: parent.id)
        try Data("parent-base-image-before-copy".utf8)
            .write(to: parentLayout.baseImageURL)
        try Data("parent-state-image-before-copy".utf8)
            .write(to: parentLayout.stateImageURL)

        _ = try registry.copyEnvironment(
            from: parent.id,
            to: "default-copy",
            rootImageIdentifier: "orlix.env.default-copy"
        )
        let copiedLayout = try registry.layout(forEnvironmentID: "default-copy")

        try Data("parent-base-image-after-copy".utf8)
            .write(to: parentLayout.baseImageURL)
        try Data("parent-state-image-after-copy".utf8)
            .write(to: parentLayout.stateImageURL)

        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.baseImageURL),
            Data("parent-base-image-before-copy".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: copiedLayout.stateImageURL),
            Data("parent-state-image-before-copy".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: parentLayout.baseImageURL),
            Data("parent-base-image-after-copy".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: parentLayout.stateImageURL),
            Data("parent-state-image-after-copy".utf8)
        )
    }

    func testEnvironmentRootImageRequiresMaterializedBaseAndStateImages() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-materialized",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-materialized",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        try registry.save(descriptor)
        let layout = try registry.layout(forEnvironmentID: descriptor.id)

        XCTAssertThrowsError(
            try registry.materializedRootImage(
                forEnvironmentID: descriptor.id,
                kernelCommandLine: "console=hvc0"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .missingImage(layout.baseImageURL.path)
            )
        }

        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try registry.materializedRootImage(
            forEnvironmentID: descriptor.id,
            kernelCommandLine: "console=hvc0"
        )

        XCTAssertEqual(rootImage.environmentID, descriptor.id)
        XCTAssertEqual(rootImage.rootImageIdentifier, descriptor.rootImageIdentifier)
        XCTAssertEqual(rootImage.baseImageURL, layout.baseImageURL)
        XCTAssertEqual(rootImage.stateImageURL, layout.stateImageURL)
        XCTAssertEqual(rootImage.bootConfig.rootImageIdentifier, descriptor.rootImageIdentifier)
        XCTAssertEqual(rootImage.bootConfig.kernelCommandLine, "console=hvc0")
    }

	func testEnvironmentRootImageDefaultsToOverlayRootCommandLine() throws {
		let root = temporaryRegistryRoot()
		let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-overlay-default",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-overlay-default",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout
        )

        XCTAssertEqual(
            rootImage.bootConfig.kernelCommandLine,
            OrlixEnvironmentRootImage.defaultKernelCommandLine
                + " \(OrlixEnvironmentRootImage.defaultExecCommandLineKey)=/bin/sh"
                + " \(OrlixEnvironmentRootImage.defaultArgumentCommandLineKeyPrefix)0=/bin/sh"
                + " \(OrlixEnvironmentRootImage.defaultEnvironmentCommandLineKeyPrefix)0=PATH=/usr/bin:/bin"
                + " \(OrlixEnvironmentRootImage.defaultWorkingDirectoryCommandLineKey)=/"
                + " \(OrlixEnvironmentRootImage.defaultUserIDCommandLineKey)=0"
                + " \(OrlixEnvironmentRootImage.defaultGroupIDCommandLineKey)=0"
        )
        XCTAssertEqual(rootImage.bootConfig.profile, .development)
        XCTAssertTrue(
            try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
                .contains("rdinit=/init")
        )
        XCTAssertTrue(
            try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
                .contains("orlix.root=overlay")
        )
        XCTAssertTrue(
            try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
                .contains("orlix.exec=/bin/sh")
		)
	}

	func testEnvironmentRootImageMaterializesUTSNameCommandLine() throws {
		let root = temporaryRegistryRoot()
		let descriptor = OrlixEnvironmentDescriptor(
			id: "alpine-uts-names",
			source: .ociLayout,
			platform: "linux/arm64",
			rootImageIdentifier: "orlix.env.alpine-uts-names",
			defaultCommand: ["/bin/sh"],
			defaultEnvironment: ["PATH": "/usr/bin:/bin"],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			hostname: "container-host",
			domainname: "example.test"
		)
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: descriptor.id,
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		try FileManager.default.createDirectory(
			at: layout.rootDirectory,
			withIntermediateDirectories: true
		)
		try Data("base".utf8).write(to: layout.baseImageURL)
		try Data("state".utf8).write(to: layout.stateImageURL)

		let rootImage = try OrlixEnvironmentRootImage.materialized(
			descriptor: descriptor,
			layout: layout
		)
		let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)

		XCTAssertTrue(commandLine.contains("orlix.hostname=container-host"))
		XCTAssertTrue(commandLine.contains("orlix.domainname=example.test"))
	}

	func testEnvironmentRootImageBindsDefaultCommandExecutableToInit() throws {
		let root = temporaryRegistryRoot()
		let descriptor = OrlixEnvironmentDescriptor(
            id: "busybox-command",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.busybox-command",
            defaultCommand: ["/bin/busybox", "sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout
        )

        XCTAssertEqual(
            rootImage.bootConfig.kernelCommandLine,
            OrlixEnvironmentRootImage.defaultKernelCommandLine
                + " \(OrlixEnvironmentRootImage.defaultExecCommandLineKey)=/bin/busybox"
                + " \(OrlixEnvironmentRootImage.defaultArgumentCommandLineKeyPrefix)0=/bin/busybox"
                + " \(OrlixEnvironmentRootImage.defaultArgumentCommandLineKeyPrefix)1=sh"
                + " \(OrlixEnvironmentRootImage.defaultEnvironmentCommandLineKeyPrefix)0=PATH=/usr/bin:/bin"
                + " \(OrlixEnvironmentRootImage.defaultWorkingDirectoryCommandLineKey)=/"
                + " \(OrlixEnvironmentRootImage.defaultUserIDCommandLineKey)=0"
                + " \(OrlixEnvironmentRootImage.defaultGroupIDCommandLineKey)=0"
        )
    }

    func testEnvironmentRootImageBindsEncodedExecutionDefaultsToInit() throws {
        let root = temporaryRegistryRoot()
        let descriptor = OrlixEnvironmentDescriptor(
            id: "encoded-command",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.encoded-command",
            defaultCommand: ["/bin/sh", "-c", "printf hello world"],
            defaultEnvironment: [
                "EMPTY": "",
                "MESSAGE": "hello world",
                "PATH": "/usr/bin:/bin"
            ],
            defaultWorkingDirectory: "/work/project",
            defaultUserID: 1000,
            defaultGroupID: 100,
            defaultSupplementaryGroups: [44, 45],
            defaultCapabilities: OrlixEnvironmentCapabilities(
                bounding: ["CAP_CHOWN", "CAP_SETUID"],
                permitted: ["CAP_CHOWN"],
                inheritable: ["CAP_SETUID"],
                effective: ["CAP_CHOWN"],
                ambient: ["CAP_SETUID"]
			),
			defaultNoNewPrivileges: true,
			defaultCloseAdditionalFds: true,
			defaultTerminal: false,
			defaultTerminalRows: 33,
			defaultTerminalColumns: 120,
			defaultOOMScoreAdjustment: -500,
        defaultScheduler: OrlixEnvironmentScheduler(policy: "SCHED_FIFO", priority: 1),
        defaultIOPriority: OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_BE", priority: 4),
        defaultCPUAffinity: OrlixEnvironmentCPUAffinity(mask: "0-1"),
        sysctls: [
            "kernel.hostname": "orlix demo",
            "net.ipv4.ip_forward": "1"
        ],
        maskedPaths: ["/proc/kcore", "/sys/firmware"],
            readonlyPaths: ["/proc/sys", "/sys"],
cgroupsPath: "/orlix/demo",
cgroupPidsLimit: 64,
		cgroupCPUMax: OrlixEnvironmentCgroupCPUMax(quotaMicros: 50_000, periodMicros: 100_000),
		cgroupCPUWeight: 39,
		cgroupMemoryMax: 268_435_456,
		cgroupIOWeight: 100,
            cgroupUnified: [
                OrlixEnvironmentCgroupUnifiedEntry(file: "cpu.weight", value: "39"),
                OrlixEnvironmentCgroupUnifiedEntry(file: "memory.max", value: "268435456")
            ],
            deviceNodes: [
                OrlixEnvironmentDeviceNode(
                    path: "/dev/orlix-null",
                    type: "c",
                    major: 1,
                    minor: 3,
                    fileMode: 0o666,
                    uid: 0,
                    gid: 0
                ),
                OrlixEnvironmentDeviceNode(
                    path: "/dev/orlix-pipe",
                    type: "p",
                    fileMode: 0o644,
                    uid: 0,
                    gid: 0
                )
			],
			namespaces: ["mount", "ipc", "uts", "cgroup"],
			namespacePaths: ["network": "/proc/1/ns/net"],
			tmpfsMounts: [
				try OrlixEnvironmentTmpfsMount(
					targetPath: "/run/oci-cache",
					readOnly: true,
					noSuid: true,
					noDev: true,
					noExec: true,
					data: "size=64m,mode=0755,uid=0,gid=0"
				)
			]
		)
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout
        )
        let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)

        XCTAssertTrue(commandLine.contains("orlix.exec=/bin/sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv0=/bin/sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv1=-c"))
        XCTAssertTrue(commandLine.contains("orlix.argv2=printf%20hello%20world"))
        XCTAssertTrue(commandLine.contains("orlix.env0=EMPTY="))
        XCTAssertTrue(commandLine.contains("orlix.env1=MESSAGE=hello%20world"))
        XCTAssertTrue(commandLine.contains("orlix.env2=PATH=/usr/bin:/bin"))
        XCTAssertTrue(commandLine.contains("orlix.cwd=/work/project"))
        XCTAssertTrue(commandLine.contains("orlix.uid=1000"))
        XCTAssertTrue(commandLine.contains("orlix.gid=100"))
        XCTAssertTrue(commandLine.contains("orlix.suppgid0=44"))
        XCTAssertTrue(commandLine.contains("orlix.suppgid1=45"))
        XCTAssertTrue(commandLine.contains("orlix.cap.bounding=CAP_CHOWN,CAP_SETUID"))
        XCTAssertTrue(commandLine.contains("orlix.cap.permitted=CAP_CHOWN"))
        XCTAssertTrue(commandLine.contains("orlix.cap.inheritable=CAP_SETUID"))
        XCTAssertTrue(commandLine.contains("orlix.cap.effective=CAP_CHOWN"))
	XCTAssertTrue(commandLine.contains("orlix.cap.ambient=CAP_SETUID"))
	XCTAssertTrue(commandLine.contains("orlix.nonewprivs=1"))
	XCTAssertTrue(commandLine.contains("orlix.closefds=1"))
	XCTAssertTrue(commandLine.contains("orlix.terminal=0"))
	XCTAssertTrue(commandLine.contains("orlix.terminal.rows=33"))
		XCTAssertTrue(commandLine.contains("orlix.terminal.cols=120"))
		XCTAssertTrue(commandLine.contains("orlix.oomscoreadj=-500"))
        XCTAssertTrue(commandLine.contains("orlix.scheduler.policy=SCHED_FIFO"))
        XCTAssertTrue(commandLine.contains("orlix.scheduler.priority=1"))
        XCTAssertTrue(commandLine.contains("orlix.ioprio.class=IOPRIO_CLASS_BE"))
        XCTAssertTrue(commandLine.contains("orlix.ioprio.priority=4"))
        XCTAssertTrue(commandLine.contains("orlix.cpuaffinity=0-1"))
        XCTAssertTrue(commandLine.contains("orlix.maskedpath0=/proc/kcore"))
        XCTAssertTrue(commandLine.contains("orlix.maskedpath1=/sys/firmware"))
        XCTAssertTrue(commandLine.contains("orlix.readonlypath0=/proc/sys"))
        XCTAssertTrue(commandLine.contains("orlix.readonlypath1=/sys"))
XCTAssertTrue(commandLine.contains("orlix.cgroups.path=/orlix/demo"))
XCTAssertTrue(commandLine.contains("orlix.cgroups.pids.max=64"))
XCTAssertTrue(commandLine.contains("orlix.cgroups.cpu.max=50000%20100000"))
		XCTAssertTrue(commandLine.contains("orlix.cgroups.cpu.weight=39"))
		XCTAssertTrue(commandLine.contains("orlix.cgroups.memory.max=268435456"))
        XCTAssertTrue(commandLine.contains("orlix.cgroups.io.weight=100"))
        XCTAssertTrue(commandLine.contains("orlix.cgroups.unified0=cpu.weight=39"))
        XCTAssertTrue(commandLine.contains("orlix.cgroups.unified1=memory.max=268435456"))
        XCTAssertTrue(commandLine.contains("orlix.device.path0=/dev/orlix-null"))
        XCTAssertTrue(commandLine.contains("orlix.device.type0=c"))
        XCTAssertTrue(commandLine.contains("orlix.device.major0=1"))
        XCTAssertTrue(commandLine.contains("orlix.device.minor0=3"))
        XCTAssertTrue(commandLine.contains("orlix.device.mode0=438"))
        XCTAssertTrue(commandLine.contains("orlix.device.uid0=0"))
        XCTAssertTrue(commandLine.contains("orlix.device.gid0=0"))
        XCTAssertTrue(commandLine.contains("orlix.device.path1=/dev/orlix-pipe"))
        XCTAssertTrue(commandLine.contains("orlix.device.type1=p"))
        XCTAssertTrue(commandLine.contains("orlix.device.major1=0"))
        XCTAssertTrue(commandLine.contains("orlix.device.minor1=0"))
        XCTAssertTrue(commandLine.contains("orlix.device.mode1=420"))
        XCTAssertTrue(commandLine.contains("orlix.device.uid1=0"))
        XCTAssertTrue(commandLine.contains("orlix.device.gid1=0"))
        XCTAssertTrue(commandLine.contains("orlix.namespace0=cgroup"))
        XCTAssertTrue(commandLine.contains("orlix.namespace1=ipc"))
		XCTAssertTrue(commandLine.contains("orlix.namespace2=mount"))
		XCTAssertTrue(commandLine.contains("orlix.namespace3=uts"))
		XCTAssertTrue(commandLine.contains("orlix.namespacepath0=network=/proc/1/ns/net"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.target=/run/oci-cache"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.readonly=1"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.nosuid=1"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.nodev=1"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.noexec=1"))
		XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.data=size=64m%2Cmode=0755%2Cuid=0%2Cgid=0"))
		XCTAssertTrue(commandLine.contains("orlix.sysctl0=kernel.hostname=orlix%20demo"))
        XCTAssertTrue(commandLine.contains("orlix.sysctl1=net.ipv4.ip_forward=1"))
    }

    func testEnvironmentRootImageEncodesLinuxPathDefaultsWithoutHostPathPolicy() throws {
        let root = temporaryRegistryRoot()
        let descriptor = OrlixEnvironmentDescriptor(
            id: "execution-linux-paths",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.execution-linux-paths",
            defaultCommand: ["/usr/bin/../bin/sh", "", "arg with space"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/work/../project",
            defaultUserID: 0,
            defaultGroupID: 0,
            defaultSupplementaryGroups: [44, 45],
            defaultUmask: 18,
            defaultRlimits: [
                OrlixEnvironmentRlimit(type: "RLIMIT_NOFILE", soft: 64, hard: 64)
            ]
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout
        )
        let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)

        XCTAssertTrue(commandLine.contains("orlix.exec=/usr/bin/../bin/sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv0=/usr/bin/../bin/sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv1="))
        XCTAssertTrue(commandLine.contains("orlix.argv2=arg%20with%20space"))
        XCTAssertTrue(commandLine.contains("orlix.cwd=/work/../project"))
        XCTAssertTrue(commandLine.contains("orlix.umask=18"))
        XCTAssertTrue(commandLine.contains("orlix.rlimit0=RLIMIT_NOFILE:64:64"))
    }

    func testEnvironmentRootImageEncodesPathLookupCommandName() throws {
        let root = temporaryRegistryRoot()
        let descriptor = OrlixEnvironmentDescriptor(
            id: "execution-path-lookup",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.execution-path-lookup",
            defaultCommand: ["sh", "-c", "printf path lookup"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout
        )
        let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)

        XCTAssertTrue(commandLine.contains("orlix.exec=sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv0=sh"))
        XCTAssertTrue(commandLine.contains("orlix.argv1=-c"))
        XCTAssertTrue(commandLine.contains("orlix.argv2=printf%20path%20lookup"))
        XCTAssertTrue(commandLine.contains("orlix.env0=PATH=/usr/bin:/bin"))
    }

    func testEnvironmentRootImageCommandLineKeysMatchInitParserContract() throws {
        let initSource = try String(
            contentsOf: try repositoryRoot()
                .appendingPathComponent("OrlixOS/Sources/init/init.c")
        )

        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_decoded(\"\(OrlixEnvironmentRootImage.defaultExecCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "snprintf(key, sizeof(key), \"\(OrlixEnvironmentRootImage.defaultArgumentCommandLineKeyPrefix)%d=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "snprintf(key, sizeof(key), \"\(OrlixEnvironmentRootImage.defaultEnvironmentCommandLineKeyPrefix)%d=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_decoded(\"\(OrlixEnvironmentRootImage.defaultWorkingDirectoryCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultUserIDCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultGroupIDCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "snprintf(key, sizeof(key), \"\(OrlixEnvironmentRootImage.defaultSupplementaryGroupCommandLineKeyPrefix)%d=\","
            )
        )
        XCTAssertTrue(initSource.contains("setgroups("))
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultNoNewPrivilegesCommandLineKey)=\","
            )
        )
        XCTAssertTrue(initSource.contains("PR_SET_NO_NEW_PRIVS"))
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultCloseAdditionalFdsCommandLineKey)=\","
            )
		)
		XCTAssertTrue(initSource.contains("close_additional_fds();"))
		XCTAssertTrue(initSource.contains("for (int fd = 3;"))
		XCTAssertTrue(
			initSource.contains(
				"read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultTerminalRowsCommandLineKey)=\","
			)
		)
		XCTAssertTrue(
			initSource.contains(
				"read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultTerminalColumnsCommandLineKey)=\","
			)
		)
		XCTAssertTrue(initSource.contains("struct winsize"))
		XCTAssertTrue(initSource.contains("TIOCSWINSZ"))
		XCTAssertTrue(
			initSource.contains(
				"read_cmdline_signed(\"\(OrlixEnvironmentRootImage.defaultOOMScoreAdjustmentCommandLineKey)=\","
            )
        )
        XCTAssertTrue(initSource.contains("\"/proc/self/oom_score_adj\""))
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_decoded(\"\(OrlixEnvironmentRootImage.defaultSchedulerPolicyCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultSchedulerPriorityCommandLineKey)=\","
            )
        )
        XCTAssertTrue(initSource.contains("sched_setscheduler("))
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_decoded(\"\(OrlixEnvironmentRootImage.defaultIOPriorityClassCommandLineKey)=\","
            )
        )
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_unsigned(\"\(OrlixEnvironmentRootImage.defaultIOPriorityPriorityCommandLineKey)=\","
            )
        )
        XCTAssertTrue(initSource.contains("SYS_ioprio_set"))
        XCTAssertTrue(
            initSource.contains(
                "read_cmdline_decoded(\"\(OrlixEnvironmentRootImage.defaultCPUAffinityCommandLineKey)=\","
            )
		)
		XCTAssertTrue(initSource.contains("sched_setaffinity("))
		XCTAssertTrue(initSource.contains("CLONE_NEWPID"))
		XCTAssertTrue(
			initSource.contains(
				"if ((config->namespace_flags & CLONE_NEWPID) != 0)"
			)
		)
		XCTAssertTrue(initSource.contains("fork_required |= CLONE_NEWPID;"))
		XCTAssertTrue(initSource.contains("die(\"fork namespace child\");"))
		XCTAssertTrue(
			initSource.contains(
				"snprintf(key, sizeof(key), \"orlix.mount.tmpfs%d.target=\", index);"
			)
		)
		XCTAssertTrue(initSource.contains("mount_configured_tmpfs_mounts();"))
		XCTAssertTrue(initSource.contains("mount_if_needed(\"tmpfs\", target, \"tmpfs\""))
		XCTAssertTrue(
			initSource.contains(
				"snprintf(key, sizeof(key), \"orlix.mount.host%d.target=\", index);"
            )
        )
	XCTAssertTrue(
		initSource.contains(
			"snprintf(key, sizeof(key), \"orlix.mount.host%d.readonly=\", index);"
		)
	)
	XCTAssertTrue(
		initSource.contains(
			"snprintf(key, sizeof(key), \"orlix.mount.host%d.noexec=\", index);"
		)
	)
	XCTAssertTrue(initSource.contains("flags |= MS_NOEXEC;"))
	XCTAssertTrue(initSource.contains("snprintf(source, sizeof(source), \"orlix-host%d\", index);"))
	XCTAssertTrue(initSource.contains("mount_if_needed(source, target, \"virtiofs\""))
        XCTAssertTrue(initSource.contains("\"virtiofs\""))
		let runtimeMountRange = try XCTUnwrap(
			initSource.range(of: "mount_runtime_filesystems();")
		)
		let hostMountRange = try XCTUnwrap(
			initSource.range(of: "mount_configured_host_directories();")
		)
		let configuredTmpfsRange = try XCTUnwrap(
			initSource.range(of: "mount_configured_tmpfs_mounts();")
		)
		let terminalFlagRange = try XCTUnwrap(
			initSource.range(of: "read_cmdline_decoded(\"orlix.terminal=\",")
		)
		let ptyRange = try XCTUnwrap(initSource.range(of: "if (run_pty_shell("))
		let ptyStartedRange = try XCTUnwrap(
			initSource.range(of: "write_process_started(shell);")
		)
		let ptyCompletionRange = try XCTUnwrap(
			initSource.range(of: "write_process_completion(shell, child_status);")
		)
		let ptyHangupReapRange = try XCTUnwrap(
			initSource.range(of: "wait_for_shell_exit(shell, &status);")
		)
		let ptyEOFRange = try XCTUnwrap(
			initSource.range(of: "copy_available_or_eof(master, STDOUT_FILENO);")
		)
		let consoleEOFRange = try XCTUnwrap(
			initSource.range(of: "copy_available_or_eof(console_fd, master);")
		)
		XCTAssertLessThan(runtimeMountRange.lowerBound, hostMountRange.lowerBound)
		XCTAssertLessThan(configuredTmpfsRange.lowerBound, hostMountRange.lowerBound)
		XCTAssertLessThan(hostMountRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(terminalFlagRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(ptyStartedRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(consoleEOFRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(ptyEOFRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(ptyHangupReapRange.lowerBound, ptyRange.lowerBound)
		XCTAssertLessThan(ptyCompletionRange.lowerBound, ptyRange.lowerBound)
		XCTAssertTrue(initSource.contains("ready = poll(fds, 2, 100);"))
		XCTAssertTrue(initSource.contains("SYS_capset"))
        XCTAssertTrue(initSource.contains("SYS_capget"))
        XCTAssertTrue(initSource.contains("PR_CAPBSET_DROP"))
        XCTAssertTrue(initSource.contains("PR_CAP_AMBIENT"))
        XCTAssertTrue(initSource.contains("CPU_SET("))
        XCTAssertTrue(
            initSource.contains("snprintf(key, sizeof(key), \"orlix.sysctl%d=\", i);")
        )
        XCTAssertTrue(initSource.contains("parse_sysctl_assignment("))
        XCTAssertTrue(initSource.contains("snprintf(path, path_size, \"/proc/sys/%s\", key);"))
        XCTAssertTrue(initSource.contains("if (*cursor == '.')"))
        XCTAssertTrue(initSource.contains("apply_sysctl(&config->sysctls[i]);"))
    }

    func testEnvironmentRootImageRejectsUnsafeDefaultCommandExecutable() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "unsafe-command",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let descriptor = OrlixEnvironmentDescriptor(
            id: "unsafe-command",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.unsafe-command",
            defaultCommand: ["bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )

        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: descriptor,
                layout: layout
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .invalidDefaultCommand("bin/sh")
            )
        }
    }

    func testEnvironmentRootImageRejectsUnsafeExecutionDefaults() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "unsafe-execution-defaults",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let badEnv = OrlixEnvironmentDescriptor(
            id: "unsafe-execution-defaults",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.unsafe-execution-defaults",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["BAD=KEY": "value"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: badEnv,
                layout: layout
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .invalidDefaultEnvironment("BAD=KEY")
            )
        }

        let badCwd = OrlixEnvironmentDescriptor(
            id: "unsafe-execution-defaults",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.unsafe-execution-defaults",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: [:],
            defaultWorkingDirectory: "work",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: badCwd,
                layout: layout
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .invalidDefaultWorkingDirectory("work")
            )
        }
    }

    func testEnvironmentRootImageRegistersDocumentsMountHostDirectory() throws {
        let root = temporaryRegistryRoot()
        let documentsRoot = root.appendingPathComponent("Documents", isDirectory: true)
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "documents-mount",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try FileManager.default.createDirectory(
            at: documentsRoot,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)
        let documentsMount = try OrlixEnvironmentMount.documents(
            targetPath: "/home/root/Documents",
            readOnly: true
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "documents-mount",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.documents-mount",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [documentsMount]
        )

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout,
            documentsDirectory: documentsRoot
        )
        XCTAssertEqual(
            rootImage.hostDirectories,
            [
                OrlixHostDirectoryRegistration(
                    identifier: OrlixEnvironmentRootImage.defaultHostDirectoryIdentifier,
                    hostPath: documentsRoot.path,
                    readOnly: true
                )
            ]
        )
        let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
        XCTAssertTrue(
            commandLine.contains(
                "\(OrlixEnvironmentRootImage.defaultHostMountTargetCommandLineKey)=/home/root/Documents"
            )
        )
        XCTAssertTrue(
            commandLine.contains(
                "\(OrlixEnvironmentRootImage.defaultHostMountReadOnlyCommandLineKey)=1"
            )
        )
    }

    func testEnvironmentRootImageMaterializesMultipleDocumentsHostMounts() throws {
        let root = temporaryRegistryRoot()
        let documentsRoot = root.appendingPathComponent("Documents", isDirectory: true)
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "multi-documents-mount",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try FileManager.default.createDirectory(
            at: documentsRoot,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let descriptor = OrlixEnvironmentDescriptor(
            id: "multi-documents-mount",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.multi-documents-mount",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [
                try OrlixEnvironmentMount.documents(
                    targetPath: "/home/root/Documents",
                    readOnly: true
                ),
                try OrlixEnvironmentMount.documents(
                    targetPath: "/mnt/shared",
                    readOnly: false
                )
            ]
        )

        let rootImage = try OrlixEnvironmentRootImage.materialized(
            descriptor: descriptor,
            layout: layout,
            documentsDirectory: documentsRoot
        )

        XCTAssertEqual(
            rootImage.hostDirectories,
            [
                OrlixHostDirectoryRegistration(
                    identifier: "orlix-host0",
                    hostPath: documentsRoot.path,
                    readOnly: true
                ),
                OrlixHostDirectoryRegistration(
                    identifier: "orlix-host1",
                    hostPath: documentsRoot.path,
                    readOnly: false
                )
            ]
        )
        let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
        XCTAssertTrue(commandLine.contains("orlix.mount.host0.target=/home/root/Documents"))
        XCTAssertTrue(commandLine.contains("orlix.mount.host0.readonly=1"))
	    XCTAssertTrue(commandLine.contains("orlix.mount.host1.target=/mnt/shared"))
	    XCTAssertFalse(commandLine.contains("orlix.mount.host1.readonly=1"))
	}

	func testEnvironmentRootImageMaterializesExternalBookmarkHostMount() throws {
		let root = temporaryRegistryRoot()
		let externalRoot = root.appendingPathComponent("Selected Project", isDirectory: true)
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: "external-bookmark-mount",
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		try FileManager.default.createDirectory(
			at: layout.rootDirectory,
			withIntermediateDirectories: true
		)
		try FileManager.default.createDirectory(
			at: externalRoot,
			withIntermediateDirectories: true
		)
		try Data("base".utf8).write(to: layout.baseImageURL)
		try Data("state".utf8).write(to: layout.stateImageURL)

		let descriptor = OrlixEnvironmentDescriptor(
			id: "external-bookmark-mount",
			source: .copiedEnvironment(parentID: "default"),
			platform: "linux/arm64",
			rootImageIdentifier: "orlix.env.external-bookmark-mount",
			defaultCommand: ["/bin/sh"],
			defaultEnvironment: ["PATH": "/usr/bin:/bin"],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			mounts: [
				try OrlixEnvironmentMount.securityScopedExternal(
					bookmarkID: "selected-project",
					targetPath: "/mnt/project",
					readOnly: true
				)
			]
		)

		let rootImage = try OrlixEnvironmentRootImage.materialized(
			descriptor: descriptor,
			layout: layout,
			securityScopedExternalDirectories: [
				"selected-project": externalRoot
			]
		)

		XCTAssertEqual(
			rootImage.hostDirectories,
			[
				OrlixHostDirectoryRegistration(
					identifier: "orlix-host0",
					hostPath: externalRoot.path,
					readOnly: true
				)
			]
		)
		let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
		XCTAssertTrue(commandLine.contains("orlix.mount.host0.target=/mnt/project"))
		XCTAssertTrue(commandLine.contains("orlix.mount.host0.readonly=1"))
	}

	func testEnvironmentRootImageCarriesRootPropagationToKernelCommandLine() throws {
		let root = temporaryRegistryRoot()
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: "shared-root-propagation",
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		try FileManager.default.createDirectory(
			at: layout.rootDirectory,
			withIntermediateDirectories: true
		)
		try Data("base".utf8).write(to: layout.baseImageURL)
		try Data("state".utf8).write(to: layout.stateImageURL)
		let descriptor = OrlixEnvironmentDescriptor(
			id: "shared-root-propagation",
			source: .ociLayout,
			platform: "linux/arm64",
			rootImageIdentifier: "orlix.env.shared-root-propagation",
			defaultCommand: ["/bin/sh"],
			defaultEnvironment: ["PATH": "/usr/bin:/bin"],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			rootPropagation: .shared
		)

		let rootImage = try OrlixEnvironmentRootImage.materialized(
			descriptor: descriptor,
			layout: layout
		)
		let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)

		XCTAssertTrue(
			commandLine.contains(
				"\(OrlixEnvironmentRootImage.rootPropagationCommandLineKey)=shared"
			)
		)
	}

	func testEnvironmentRootImageRejectsMountsWithoutLinuxBackend() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "unsupported-mounts",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let externalMount = try OrlixEnvironmentMount.securityScopedExternal(
            bookmarkID: "project-folder-bookmark",
            targetPath: "/mnt/external/project",
            readOnly: true
        )
        let externalDescriptor = OrlixEnvironmentDescriptor(
            id: "unsupported-mounts",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.unsupported-mounts",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [externalMount]
        )

        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: externalDescriptor,
                layout: layout
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .missingLinuxMountBackend(externalMount)
            )
        }

        let documentsMount = try OrlixEnvironmentMount.documents(
            targetPath: "/home/root/Documents"
        )
        let multiMountDescriptor = OrlixEnvironmentDescriptor(
            id: "unsupported-mounts",
            source: .copiedEnvironment(parentID: "default"),
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.unsupported-mounts",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            mounts: [documentsMount, externalMount]
        )

        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: multiMountDescriptor,
                layout: layout
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .missingLinuxMountBackend(externalMount)
            )
        }
    }

    func testEnvironmentRootImageRejectsMismatchedLayoutAndDirectories() throws {
        let root = temporaryRegistryRoot()
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-materialized",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-materialized",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "other-env",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )

        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: descriptor,
                layout: layout,
                kernelCommandLine: "console=hvc0"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .environmentMismatch(
                    descriptorID: "alpine-materialized",
                    layoutID: "other-env"
                )
            )
        }

        let matchingLayout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        try FileManager.default.createDirectory(
            at: matchingLayout.baseImageURL,
            withIntermediateDirectories: true
        )
        try Data("state".utf8).write(to: matchingLayout.stateImageURL)

        XCTAssertThrowsError(
            try OrlixEnvironmentRootImage.materialized(
                descriptor: descriptor,
                layout: matchingLayout,
                kernelCommandLine: "console=hvc0"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentRootImageError,
                .imageIsDirectory(matchingLayout.baseImageURL.path)
            )
        }
    }

    func testLinuxSessionCanBindMaterializedEnvironmentRootImage() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-session",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-session",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        try registry.save(descriptor)
        let layout = try registry.layout(forEnvironmentID: descriptor.id)
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let rootImage = try registry.materializedRootImage(
            forEnvironmentID: descriptor.id,
            kernelCommandLine: "console=hvc0 root=/dev/vda rootfstype=ext4 ro"
        )
        let session = OrlixLinuxSession(materializedRootImage: rootImage)

        XCTAssertEqual(session.bootConfig, rootImage.bootConfig)
        XCTAssertEqual(
            session.bootConfig.rootImageIdentifier,
            "orlix.env.alpine-session"
        )
        XCTAssertEqual(
            session.bootConfig.kernelCommandLine,
            "console=hvc0 root=/dev/vda rootfstype=ext4 ro"
        )
    }

    func testLinuxSessionCanSelectNamedEnvironmentRootFromRegistry() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-named-session",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-named-session",
            defaultCommand: ["/bin/sh", "-l"],
            defaultEnvironment: ["HOME": "/root", "PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/root",
            defaultUserID: 1000,
            defaultGroupID: 100
        )
        try registry.save(descriptor)
        let layout = try registry.layout(forEnvironmentID: descriptor.id)
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state".utf8).write(to: layout.stateImageURL)

        let session = try OrlixLinuxSession(
            environmentID: descriptor.id,
            registry: registry,
            kernelCommandLine: "console=hvc0"
        )

        XCTAssertEqual(
            session.bootConfig.rootImageIdentifier,
            descriptor.rootImageIdentifier
        )
        XCTAssertEqual(session.bootConfig.kernelCommandLine, "console=hvc0")
        XCTAssertEqual(session.bootConfig.profile, OrlixBootProfile.development)
    }

    func testNamedEnvironmentSessionReusesPersistedStateImageAfterMutation()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-persistent-session",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-persistent-session",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        try registry.save(descriptor)
        let layout = try registry.layout(forEnvironmentID: descriptor.id)
        try Data("base".utf8).write(to: layout.baseImageURL)
        try Data("state-before".utf8).write(to: layout.stateImageURL)

        let firstSession = try OrlixLinuxSession(
            environmentID: descriptor.id,
            registry: registry,
            kernelCommandLine: "console=hvc0"
        )
        let firstRootImage = try XCTUnwrap(
            firstSession.materializedRootImageForTesting
        )
        try Data("state-after-mutation".utf8).write(to: layout.stateImageURL)

        let secondSession = try OrlixLinuxSession(
            environmentID: descriptor.id,
            registry: registry,
            kernelCommandLine: "console=hvc0"
        )
        let secondRootImage = try XCTUnwrap(
            secondSession.materializedRootImageForTesting
        )

        XCTAssertEqual(firstRootImage.baseImageURL, layout.baseImageURL)
        XCTAssertEqual(firstRootImage.stateImageURL, layout.stateImageURL)
        XCTAssertEqual(secondRootImage.baseImageURL, layout.baseImageURL)
        XCTAssertEqual(secondRootImage.stateImageURL, layout.stateImageURL)
        XCTAssertEqual(try Data(contentsOf: layout.baseImageURL), Data("base".utf8))
        XCTAssertEqual(
            try Data(contentsOf: secondRootImage.stateImageURL),
            Data("state-after-mutation".utf8)
        )
    }

    func testMaterializedEnvironmentRootImageRegistersWithHostAdapter() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let descriptor = OrlixEnvironmentDescriptor(
            id: "alpine-register",
            source: .rootfsTar,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.env.alpine-register",
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: ["PATH": "/usr/bin:/bin"],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0
        )
        try registry.save(descriptor)
        let layout = try registry.layout(forEnvironmentID: descriptor.id)
        try Data(repeating: 0, count: 512).write(to: layout.baseImageURL)
        try Data(repeating: 0, count: 512).write(to: layout.stateImageURL)

        let rootImage = try registry.materializedRootImage(
            forEnvironmentID: descriptor.id,
            kernelCommandLine: "console=hvc0 root=/dev/vda rootfstype=ext4 ro"
        )

        let payloadRoot = root.appendingPathComponent(
            "Payload.bundle",
            isDirectory: true
        )
        try FileManager.default.createDirectory(
            at: payloadRoot,
            withIntermediateDirectories: true
        )

        XCTAssertTrue(
            rootImage.registerWithHostAdapterForTesting(
                payloadBundlePath: payloadRoot.path,
                initrdResource: "rootfs/initramfs.cpio.gz",
                baseBlockDevice: 0,
                stateBlockDevice: 1,
                stateBlockMinimumBytes: 1024
            )
        )
    }

    func testMaterializedRootRegistrationDoesNotRegisterAllBundledRootsFirst() throws {
        let sourceRoot = try repositoryRoot()
        let orlixOSSource = try String(
            contentsOf: sourceRoot
                .appendingPathComponent("OrlixOS/Sources/Session/OrlixOS.swift")
        )
        let methodRange = try XCTUnwrap(
            orlixOSSource.range(
                of: "static func registerMaterializedRootImage("
            )
        )
        let productResourcesRange = try XCTUnwrap(
            orlixOSSource[methodRange.lowerBound...].range(
                of: "private static var productResources"
            )
        )
        let methodBody = orlixOSSource[methodRange.lowerBound..<productResourcesRange.lowerBound]

        XCTAssertFalse(methodBody.contains("registerWithHostAdapter()"))
        XCTAssertTrue(methodBody.contains("orlix_host_resources_set_payload_root_path"))
        XCTAssertTrue(methodBody.contains("orlix_host_resources_clear_root_images()"))
        XCTAssertTrue(methodBody.contains("registerHostDirectories(rootImage.hostDirectories)"))
        XCTAssertTrue(methodBody.contains("orlix_host_resources_clear_host_directories()"))
        XCTAssertTrue(methodBody.contains("orlix_host_resources_register_root_image_files"))
    }

    func testProductRootRegistrationClearsStaleHostDirectories() throws {
        let sourceRoot = try repositoryRoot()
        let orlixOSSource = try String(
            contentsOf: sourceRoot
                .appendingPathComponent("OrlixOS/Sources/Session/OrlixOS.swift")
        )
        let methodRange = try XCTUnwrap(
            orlixOSSource.range(of: "static func registerWithHostAdapter()")
        )
        let materializedRange = try XCTUnwrap(
            orlixOSSource[methodRange.lowerBound...].range(
                of: "static func registerMaterializedRootImage("
            )
        )
        let methodBody = orlixOSSource[methodRange.lowerBound..<materializedRange.lowerBound]

        XCTAssertTrue(methodBody.contains("orlix_host_resources_clear_root_images()"))
        XCTAssertTrue(methodBody.contains("orlix_host_resources_clear_host_directories()"))
        XCTAssertFalse(methodBody.contains("registerHostDirectories("))
    }

    func testEnvironmentImageMaterializationPlanMatchesProductExt4Shape() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "alpine-ext4",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let stagingRoot = layout.importScratchDirectory
            .appendingPathComponent("staging-root", isDirectory: true)
        let sourceEtcModificationDate = Date(timeIntervalSince1970: 1_234_567_890)
        try FileManager.default.createDirectory(
            at: stagingRoot.appendingPathComponent("etc", isDirectory: true),
            withIntermediateDirectories: true
        )
        try Data("ID=alpine\n".utf8).write(
            to: stagingRoot.appendingPathComponent("etc/os-release")
        )
        try FileManager.default.setAttributes(
            [
                .posixPermissions: NSNumber(value: 0o701),
                .modificationDate: sourceEtcModificationDate
            ],
            ofItemAtPath: stagingRoot.appendingPathComponent("etc").path
        )

        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: stagingRoot,
            storageLayout: layout
        )
        try plan.prepareInputTrees()

        XCTAssertEqual(plan.baseTreeDirectory.lastPathComponent, "base-tree")
        XCTAssertEqual(plan.stateTreeDirectory.lastPathComponent, "state-tree")
        XCTAssertEqual(plan.baseImageURL, layout.baseImageURL)
        XCTAssertEqual(plan.stateImageURL, layout.stateImageURL)
        XCTAssertEqual(plan.baseLabel, "ORLIXROOT")
        XCTAssertEqual(plan.stateLabel, "ORLIXSTATE")
        XCTAssertEqual(plan.rootOwner, "0:0")
        XCTAssertEqual(
            try String(
                contentsOf: plan.baseTreeDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        let copiedEtcAttributes = try FileManager.default.attributesOfItem(
            atPath: plan.baseTreeDirectory.appendingPathComponent("etc").path
        )
        XCTAssertEqual(
            copiedEtcAttributes[.posixPermissions] as? NSNumber,
            NSNumber(value: 0o701)
        )
        let copiedEtcModificationDate = try XCTUnwrap(
            copiedEtcAttributes[.modificationDate] as? Date
        )
        XCTAssertLessThan(
            abs(copiedEtcModificationDate.timeIntervalSince(sourceEtcModificationDate)),
            1.0
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.baseTreeDirectory
                    .appendingPathComponent("dev").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.baseTreeDirectory
                    .appendingPathComponent("proc").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.baseTreeDirectory
                    .appendingPathComponent("sys").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.baseTreeDirectory
                    .appendingPathComponent("tmp").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.stateTreeDirectory
                    .appendingPathComponent("upper").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: plan.stateTreeDirectory
                    .appendingPathComponent("work").path
            )
        )
    }

    func testEnvironmentImageMaterializationCommandsUseCanonicalMke2fsFlags() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "alpine-ext4",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: layout.importScratchDirectory
                .appendingPathComponent("staging-root", isDirectory: true),
            storageLayout: layout,
            baseImageSize: "128m",
            stateImageSize: "64m"
        )

        let commands = try plan.commands(
            mke2fsExecutable: "/opt/e2fsprogs/bin/mke2fs",
            truncateExecutable: "/usr/bin/truncate",
            debugfsExecutable: "/opt/e2fsprogs/sbin/debugfs"
        )

        XCTAssertEqual(
            commands,
            [
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/usr/bin/truncate",
                    arguments: ["-s", "128m", layout.baseImageURL.path]
                ),
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/opt/e2fsprogs/bin/mke2fs",
                    arguments: [
                        "-q",
                        "-t",
                        "ext4",
                        "-F",
                        "-m",
                        "0",
                        "-U",
                        "clear",
                        "-L",
                        "ORLIXROOT",
                        "-E",
                        "root_owner=0:0",
                        "-d",
                        plan.baseTreeDirectory.path,
                        layout.baseImageURL.path
                    ]
                ),
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/opt/e2fsprogs/sbin/debugfs",
                    arguments: [
                        "-w",
                        "-f",
                        plan.baseMetadataCommandsURL.path,
                        layout.baseImageURL.path
                    ]
                ),
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/usr/bin/truncate",
                    arguments: ["-s", "64m", layout.stateImageURL.path]
                ),
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/opt/e2fsprogs/bin/mke2fs",
                    arguments: [
                        "-q",
                        "-t",
                        "ext4",
                        "-F",
                        "-m",
                        "0",
                        "-U",
                        "clear",
                        "-L",
                        "ORLIXSTATE",
                        "-E",
                        "root_owner=0:0",
                        "-d",
                        plan.stateTreeDirectory.path,
                        layout.stateImageURL.path
                    ]
                ),
                OrlixEnvironmentImageMaterializationCommand(
                    executable: "/opt/e2fsprogs/sbin/debugfs",
                    arguments: [
                        "-w",
                        "-f",
                        plan.stateMetadataCommandsURL.path,
                        layout.stateImageURL.path
                    ]
                )
            ]
        )
	}

	func testEnvironmentImageMaterializationRunnerExecutesCanonicalCommandsInOrder() throws {
		let root = temporaryRegistryRoot()
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: "alpine-ext4-runner",
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
			stagingRootDirectory: layout.importScratchDirectory
				.appendingPathComponent("staging-root", isDirectory: true),
			storageLayout: layout,
			baseImageSize: "128m",
			stateImageSize: "64m"
		)
		let runner = RecordingMaterializationCommandRunner()

		let result = try plan.materialize(
			mke2fsExecutable: "/opt/e2fsprogs/bin/mke2fs",
			truncateExecutable: "/usr/bin/truncate",
			debugfsExecutable: "/opt/e2fsprogs/sbin/debugfs",
			runner: runner
		)

		let expectedCommands = try plan.commands(
			mke2fsExecutable: "/opt/e2fsprogs/bin/mke2fs",
			truncateExecutable: "/usr/bin/truncate",
			debugfsExecutable: "/opt/e2fsprogs/sbin/debugfs"
		)
		XCTAssertEqual(result.commands, expectedCommands)
		XCTAssertEqual(runner.commands, expectedCommands)
	}

	func testEnvironmentImageMaterializationRunnerStopsAtFailingCommand() throws {
		let root = temporaryRegistryRoot()
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: "alpine-ext4-runner-fail",
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
			stagingRootDirectory: layout.importScratchDirectory
				.appendingPathComponent("staging-root", isDirectory: true),
			storageLayout: layout
		)
		let expectedCommands = try plan.commands()
		let runner = RecordingMaterializationCommandRunner(failingCommandIndex: 2)

		XCTAssertThrowsError(try plan.materialize(runner: runner)) { error in
			XCTAssertEqual(
				error as? RecordingMaterializationCommandRunnerError,
				.requestedFailure(expectedCommands[2])
			)
		}
		XCTAssertEqual(runner.commands, Array(expectedCommands.prefix(2)))
	}

	func testEnvironmentRootImageMakeTargetConsumesImporterMetadataCommands()
        throws
    {
        let sourceRoot = try repositoryRoot()
        let makefile = try String(
            contentsOf: sourceRoot.appendingPathComponent("OrlixOS/Makefile")
        )

        XCTAssertTrue(makefile.contains("ORLIXOS_ENVIRONMENT_BASE_DEBUGFS_COMMANDS"))
        XCTAssertTrue(makefile.contains("ORLIXOS_ENVIRONMENT_STATE_DEBUGFS_COMMANDS"))
        XCTAssertTrue(makefile.contains("missing ORLIXOS_ENVIRONMENT_STATE_DEBUGFS_COMMANDS"))
        XCTAssertTrue(makefile.contains(#"state_metadata_commands="$$work_root/state-debugfs-metadata.commands""#))
        XCTAssertTrue(makefile.contains(#"ORLIXOS_ENVIRONMENT_STATE_DEBUGFS_COMMANDS="$$state_metadata_commands""#))
        XCTAssertTrue(makefile.contains("set_inode_field %s mode 040755"))
    }

    func testEnvironmentImageMaterializationGeneratesManifestMetadataCommands() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "alpine-ext4",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: layout.importScratchDirectory
                .appendingPathComponent("staging-root", isDirectory: true),
            storageLayout: layout
        )

        let commands = try plan.baseImageMetadataCommands(
            manifest: [
                OrlixRootfsTarManifestEntry(
                    path: "etc",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .directory,
                    linkName: nil
                ),
                OrlixRootfsTarManifestEntry(
                    path: #"home/orlix/has "quotes""#,
                    size: 4,
                    mode: 0o644,
                    uid: 1000,
                    gid: 1000,
                    type: .regularFile,
                    linkName: nil
                )
            ]
        )

        XCTAssertEqual(
            commands,
            [
                #"set_inode_field "/etc" uid 0"#,
                #"set_inode_field "/etc" gid 0"#,
                #"set_inode_field "/etc" mode 040755"#,
                #"set_inode_field "/etc" mtime 0"#,
                #"set_inode_field "/home/orlix/has \"quotes\"" uid 1000"#,
                #"set_inode_field "/home/orlix/has \"quotes\"" gid 1000"#,
                #"set_inode_field "/home/orlix/has \"quotes\"" mode 0100644"#,
                #"set_inode_field "/home/orlix/has \"quotes\"" mtime 0"#
            ]
        )
    }

    func testEnvironmentImageMaterializationRejectsUnsafeInputs() throws {
        let root = temporaryRegistryRoot()
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "alpine-ext4",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )

        XCTAssertThrowsError(
            try OrlixEnvironmentImageMaterializationPlan.plan(
                stagingRootDirectory: layout.importScratchDirectory,
                storageLayout: layout,
                baseImageSize: "../64m"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentImageMaterializationError,
                .invalidImageSize("../64m")
            )
        }

        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: layout.importScratchDirectory,
            storageLayout: layout
        )
        XCTAssertThrowsError(
            try plan.commands(mke2fsExecutable: "")
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentImageMaterializationError,
                .invalidExecutable("")
            )
        }
        XCTAssertThrowsError(
            try plan.commands(debugfsExecutable: "")
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentImageMaterializationError,
                .invalidExecutable("")
            )
        }

        XCTAssertThrowsError(
            try plan.baseImageMetadataCommands(
                manifest: [
                    OrlixRootfsTarManifestEntry(
                        path: "bad\npath",
                        size: 0,
                        mode: 0o644,
                        uid: 0,
                        gid: 0,
                        type: .regularFile,
                        linkName: nil
                    )
                ]
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixEnvironmentImageMaterializationError,
                .invalidDebugfsPath("bad\npath")
            )
        }
    }

    func testRootfsTarEntryPathPolicyNormalizesSafeRelativePaths() throws {
        XCTAssertEqual(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("./etc/os-release"),
            "etc/os-release"
        )
        XCTAssertEqual(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("usr/bin/sh"),
            "usr/bin/sh"
        )
        XCTAssertEqual(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("./"),
            nil
        )
    }

    func testRootfsTarEntryPathPolicyRejectsUnsafePaths() {
        XCTAssertThrowsError(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("")
        )
        XCTAssertThrowsError(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("/etc/passwd")
        )
        XCTAssertThrowsError(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("../etc/passwd")
        )
        XCTAssertThrowsError(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("etc/../passwd")
        )
        XCTAssertThrowsError(
            try OrlixRootfsTarEntryPathPolicy.normalizedRelativePath("etc/passwd\u{0}")
        )
    }

    func testRootfsTarImportPlanBindsArchiveToEnvironmentStorage() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let archive = root.appendingPathComponent("alpine-rootfs.tar")

        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: archive,
            environmentID: "alpine-tar",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-tar"
        )

        XCTAssertEqual(plan.archiveURL, archive)
        XCTAssertEqual(plan.descriptor.id, "alpine-tar")
        XCTAssertEqual(plan.descriptor.source, .rootfsTar)
        XCTAssertEqual(plan.descriptor.platform, "linux/arm64")
        XCTAssertEqual(plan.descriptor.rootImageIdentifier, "orlix.env.alpine-tar")
        XCTAssertEqual(plan.storageLayout.environmentID, "alpine-tar")
        XCTAssertTrue(
            plan.storageLayout.baseImageURL.path.hasSuffix(
                "Application Support/Orlix/environments/alpine-tar/base.ext4"
            )
        )
    }

    func testRootfsTarManifestReaderParsesUstarEntries() throws {
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8)),
            TarFixtureEntry(path: "bin/sh", type: "2", linkName: "/usr/bin/busybox"),
            TarFixtureEntry(
                path: "sbin/init",
                type: "1",
                linkName: "bin/sh",
                modificationTime: 1_710_000_000
            )
        ])

        let entries = try OrlixRootfsTarManifestReader().readManifest(from: data)

        XCTAssertEqual(
            entries,
            [
                OrlixRootfsTarManifestEntry(
                    path: "etc",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .directory,
                    linkName: nil
                ),
                OrlixRootfsTarManifestEntry(
                    path: "etc/os-release",
                    size: 10,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .regularFile,
                    linkName: nil
                ),
                OrlixRootfsTarManifestEntry(
                    path: "bin/sh",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .symbolicLink,
                    linkName: "/usr/bin/busybox"
                ),
                OrlixRootfsTarManifestEntry(
                    path: "sbin/init",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .hardLink,
                    linkName: "bin/sh",
                    modificationTime: 1_710_000_000
                )
            ]
        )
    }

    func testRootfsTarManifestReaderParsesLinuxSpecialFiles() throws {
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "dev/null",
                type: "3",
                devMajor: 1,
                devMinor: 3
            ),
            TarFixtureEntry(
                path: "dev/vda",
                type: "4",
                devMajor: 254,
                devMinor: 0
            ),
            TarFixtureEntry(path: "run/initctl", type: "6")
        ])

        let entries = try OrlixRootfsTarManifestReader().readManifest(from: data)

        XCTAssertEqual(
            entries,
            [
                OrlixRootfsTarManifestEntry(
                    path: "dev/null",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .characterDevice,
                    linkName: nil,
                    deviceMajor: 1,
                    deviceMinor: 3
                ),
                OrlixRootfsTarManifestEntry(
                    path: "dev/vda",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .blockDevice,
                    linkName: nil,
                    deviceMajor: 254,
                    deviceMinor: 0
                ),
                OrlixRootfsTarManifestEntry(
                    path: "run/initctl",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .fifo,
                    linkName: nil
                )
            ]
        )
    }

    func testRootfsTarManifestReaderAppliesPAXExtendedHeaders() throws {
        let longPath = "usr/share/orlix/" + String(repeating: "component", count: 12)
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/long-file",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": longPath,
                    "uid": "1000",
                    "gid": "100",
                    "mode": "0640",
                    "mtime": "1710001234.987654321",
                    "SCHILY.xattr.security.selinux": "system_u:object_r:usr_t:s0",
                    "SCHILY.xattr.user.comment": "hello world",
                    "LIBARCHIVE.xattr.user.libarchive%2Ecomment": "bGliYXJjaGl2ZSB2YWx1ZQ=="
                ])
            ),
            TarFixtureEntry(path: "long-file", payload: Data("hello\n".utf8)),
            TarFixtureEntry(
                path: "PaxHeaders.0/tool",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "bin/tool",
                    "linkpath": "/usr/bin/busybox"
                ])
            ),
            TarFixtureEntry(path: "tool", type: "2", linkName: "ignored")
        ])

        let entries = try OrlixRootfsTarManifestReader().readManifest(from: data)

        XCTAssertEqual(
            entries,
            [
                OrlixRootfsTarManifestEntry(
                    path: longPath,
                    size: 6,
                    mode: 0o640,
                    uid: 1000,
                    gid: 100,
                    type: .regularFile,
                    linkName: nil,
                    modificationTime: 1_710_001_234,
                    extendedAttributes: [
                        "user.libarchive.comment": "libarchive value",
                        "security.selinux": "system_u:object_r:usr_t:s0",
                        "user.comment": "hello world"
                    ]
                ),
                OrlixRootfsTarManifestEntry(
                    path: "bin/tool",
                    size: 0,
                    mode: 0o755,
                    uid: 0,
                    gid: 0,
                    type: .symbolicLink,
                    linkName: "/usr/bin/busybox"
                )
            ]
        )
    }

    func testRootfsTarMaterializerCarriesPAXExtendedAttributesIntoMetadata() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/os-release",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "etc/os-release",
                    "LIBARCHIVE.xattr.user.libarchive%2Ecomment": "bGliYXJjaGl2ZSB2YWx1ZQ==",
                    "SCHILY.xattr.security.selinux": "system_u:object_r:etc_t:s0",
                    "SCHILY.xattr.user.comment": "hello world"
                ])
            ),
            TarFixtureEntry(path: "os-release", payload: Data("ID=alpine\n".utf8))
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(data, into: root)

        XCTAssertEqual(entries.first?.path, "etc/os-release")
        XCTAssertEqual(
            entries.first?.extendedAttributes,
            [
                "security.selinux": "system_u:object_r:etc_t:s0",
                "user.comment": "hello world",
                "user.libarchive.comment": "libarchive value"
            ]
        )

        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "xattr-files",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: root,
            storageLayout: layout
        )
        let commands = try plan.baseImageMetadataCommands(manifest: entries)

        XCTAssertTrue(
            commands.contains(
                #"ea_set "/etc/os-release" security.selinux "system_u:object_r:etc_t:s0""#
            )
        )
        XCTAssertTrue(
            commands.contains(#"ea_set "/etc/os-release" user.comment "hello world""#)
        )
        XCTAssertTrue(
            commands.contains(
                #"ea_set "/etc/os-release" user.libarchive.comment "libarchive value""#
            )
        )
    }

    func testRootfsTarMaterializerAppliesGNUSparsePAXMap() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/sparse-file",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "var/lib/orlix/sparse-file",
                    "GNU.sparse.map": "0,5,12,4",
                    "GNU.sparse.size": "16"
                ])
            ),
            TarFixtureEntry(
                path: "sparse-file",
                payload: Data("helloTAIL".utf8)
            )
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(data, into: root)
        let fileData = try Data(
            contentsOf: root.appendingPathComponent("var/lib/orlix/sparse-file")
        )

        XCTAssertEqual(entries.first?.path, "var/lib/orlix/sparse-file")
        XCTAssertEqual(
            entries.first?.sparseExtents,
            [
                OrlixRootfsTarSparseExtent(offset: 0, length: 5),
                OrlixRootfsTarSparseExtent(offset: 12, length: 4)
            ]
        )
        XCTAssertEqual(entries.first?.logicalSize, 16)
        XCTAssertEqual(fileData.count, 16)
        XCTAssertEqual(Array(fileData[0..<5]), Array(Data("hello".utf8)))
        XCTAssertEqual(Array(fileData[5..<12]), Array(repeating: 0, count: 7))
        XCTAssertEqual(Array(fileData[12..<16]), Array(Data("TAIL".utf8)))
    }

    func testRootfsTarMaterializerRejectsGNUSparsePAXMapOutsideLogicalSize() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/sparse-file",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "var/lib/orlix/sparse-file",
                    "GNU.sparse.map": "12,8",
                    "GNU.sparse.size": "16"
                ])
            ),
            TarFixtureEntry(
                path: "sparse-file",
                payload: Data("toolong!".utf8)
            )
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarMaterializer().materialize(data, into: root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixRootfsTarManifestError,
                .invalidPAXExtendedHeader
            )
        }
        XCTAssertFalse(FileManager.default.fileExists(atPath: root.path))
    }

    func testRootfsTarMaterializerRejectsGNUSparsePAXPayloadLengthMismatch() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/sparse-file",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "var/lib/orlix/sparse-file",
                    "GNU.sparse.map": "0,3",
                    "GNU.sparse.size": "3"
                ])
            ),
            TarFixtureEntry(
                path: "sparse-file",
                payload: Data("abcd".utf8)
            )
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarMaterializer().materialize(data, into: root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixRootfsTarManifestError,
                .invalidPAXExtendedHeader
            )
        }
        XCTAssertFalse(FileManager.default.fileExists(atPath: root.path))
    }

    func testRootfsTarMaterializerAppliesPAXPathBeforeWriting() throws {
        let root = temporaryRegistryRoot()
        let paxPath = "opt/orlix/pax-materialized"
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/file",
                type: "x",
                payload: paxExtendedHeaderPayload(["path": paxPath])
            ),
            TarFixtureEntry(path: "file", payload: Data("from-pax\n".utf8))
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(data, into: root)

        XCTAssertEqual(entries.map(\.path), [paxPath])
        XCTAssertEqual(
            try String(contentsOf: root.appendingPathComponent(paxPath)),
            "from-pax\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("file").path
            )
        )
    }

    func testRootfsTarMaterializerAppliesGNULongPathAndLinkBeforeWriting() throws {
        let root = temporaryRegistryRoot()
        let longPath = "usr/share/orlix/" + String(repeating: "long-path/", count: 9)
            + "payload"
        let longLink = "/usr/bin/" + String(repeating: "busybox-", count: 12)
            + "target"
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "././@LongLink",
                type: "L",
                payload: gnuLongNamePayload(longPath)
            ),
            TarFixtureEntry(path: "payload", payload: Data("gnu-path\n".utf8)),
            TarFixtureEntry(
                path: "././@LongLink",
                type: "K",
                payload: gnuLongNamePayload(longLink)
            ),
            TarFixtureEntry(path: "tool", type: "2", linkName: "ignored")
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(data, into: root)

        XCTAssertEqual(entries.map(\.path), [longPath, "tool"])
        XCTAssertEqual(
            try String(contentsOf: root.appendingPathComponent(longPath)),
            "gnu-path\n"
        )
        XCTAssertEqual(
            try FileManager.default.destinationOfSymbolicLink(
                atPath: root.appendingPathComponent("tool").path
            ),
            longLink
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("payload").path
            )
        )
    }

    func testRootfsTarMaterializerCarriesSpecialFilesAsImageMetadataOnly() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "dev/", type: "5"),
            TarFixtureEntry(
                path: "dev/null",
                type: "3",
                devMajor: 1,
                devMinor: 3
            ),
            TarFixtureEntry(path: "run/initctl", type: "6")
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(data, into: root)

        XCTAssertEqual(entries.map(\.path), ["dev", "dev/null", "run/initctl"])
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("dev").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("dev/null").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("run/initctl").path
            )
        )

        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: "special-files",
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent("Caches/Orlix", isDirectory: true),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixEnvironmentImageMaterializationPlan.plan(
            stagingRootDirectory: root,
            storageLayout: layout
        )
        let commands = try plan.baseImageMetadataCommands(manifest: entries)

        XCTAssertTrue(commands.contains("cd /dev"))
        XCTAssertTrue(commands.contains("mknod null c 1 3"))
        XCTAssertTrue(commands.contains("set_inode_field /dev/null mode 020755"))
        XCTAssertTrue(commands.contains("cd /run"))
        XCTAssertTrue(commands.contains("mknod initctl p"))
        XCTAssertTrue(
            commands.contains("set_inode_field /run/initctl mode 010755")
        )
    }

    func testRootfsTarManifestReaderParsesUstarPrefixPaths() throws {
        let payload = Data("prefixed\n".utf8)
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "os-release",
                prefix: "usr/lib",
                payload: payload,
                uid: 1000,
                gid: 100
            )
        ])

        let entries = try OrlixRootfsTarManifestReader().readManifest(from: data)

        XCTAssertEqual(
            entries,
            [
                OrlixRootfsTarManifestEntry(
                    path: "usr/lib/os-release",
                    size: UInt64(payload.count),
                    mode: 0o755,
                    uid: 1000,
                    gid: 100,
                    type: .regularFile,
                    linkName: nil
                )
            ]
        )
    }

    func testRootfsTarManifestReaderParsesBase256NumericFields() throws {
        let payload = Data("base256\n".utf8)
        let data = tarArchive(entries: [
            TarFixtureEntry(
                path: "var/lib/orlix/base256",
                payload: payload,
                sizeEncoding: .base256,
                uid: 131_072,
                gid: 262_144,
                numericEncoding: .base256
            )
        ])

        let entries = try OrlixRootfsTarManifestReader().readManifest(from: data)

        XCTAssertEqual(
            entries,
            [
                OrlixRootfsTarManifestEntry(
                    path: "var/lib/orlix/base256",
                    size: UInt64(payload.count),
                    mode: 0o755,
                    uid: 131_072,
                    gid: 262_144,
                    type: .regularFile,
                    linkName: nil
                )
            ]
        )
    }

    func testRootfsTarManifestReaderRejectsUnsafeArchivePaths() {
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "../etc/passwd")
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarManifestReader().readManifest(from: data)
        )
    }

    func testRootfsTarManifestReaderRejectsUnsupportedTypesAndBadChecksums() {
        let unsupportedType = tarArchive(entries: [
            TarFixtureEntry(path: "unsupported", type: "7")
        ])
        XCTAssertThrowsError(
            try OrlixRootfsTarManifestReader().readManifest(from: unsupportedType)
        )

        var badChecksum = tarArchive(entries: [
            TarFixtureEntry(path: "etc/os-release")
        ])
        badChecksum[0] = UInt8(ascii: "x")
        XCTAssertThrowsError(
            try OrlixRootfsTarManifestReader().readManifest(from: badChecksum)
        )
    }

    func testRootfsTarMaterializerWritesValidatedEntriesToStagingTree() throws {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8)),
            TarFixtureEntry(path: "bin/sh", type: "2", linkName: "/usr/bin/busybox"),
            TarFixtureEntry(path: "etc/os-release-copy", type: "1", linkName: "etc/os-release")
        ])

        let entries = try OrlixRootfsTarMaterializer().materialize(
            data,
            into: root
        )

        XCTAssertEqual(entries.count, 4)
        XCTAssertEqual(
            try String(
                contentsOf: root.appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        XCTAssertEqual(
            try FileManager.default.destinationOfSymbolicLink(
                atPath: root.appendingPathComponent("bin/sh").path
            ),
            "/usr/bin/busybox"
        )

        let originalAttributes = try FileManager.default.attributesOfItem(
            atPath: root.appendingPathComponent("etc/os-release").path
        )
        let hardlinkAttributes = try FileManager.default.attributesOfItem(
            atPath: root.appendingPathComponent("etc/os-release-copy").path
        )
        XCTAssertEqual(
            originalAttributes[.systemFileNumber] as? NSNumber,
            hardlinkAttributes[.systemFileNumber] as? NSNumber
        )
    }

    func testRootfsTarMaterializerRejectsUnsafeEntriesBeforeWriting() {
        let root = temporaryRegistryRoot()
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "../escape")
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarMaterializer().materialize(data, into: root)
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("escape").path
            )
        )
    }

    func testRootfsTarMaterializerDoesNotLeavePartialRootOnMaterializationFailure() {
        let root = temporaryRegistryRoot()
            .appendingPathComponent("rootfs", isDirectory: true)
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8)),
            TarFixtureEntry(
                path: "etc/missing-copy",
                type: "1",
                linkName: "etc/does-not-exist"
            )
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarMaterializer().materialize(data, into: root)
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: root.appendingPathComponent("etc/os-release").path
            )
        )
        XCTAssertFalse(FileManager.default.fileExists(atPath: root.path))
        XCTAssertTrue(
            (try? FileManager.default.contentsOfDirectory(
                at: root.deletingLastPathComponent(),
                includingPropertiesForKeys: nil
            ).isEmpty) ?? false
        )
    }

    func testRootfsTarImporterStagesRootfsAndPersistsEnvironmentDescriptor() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: root.appendingPathComponent("alpine-rootfs.tar"),
            environmentID: "alpine-import",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-import"
        )
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])

        let result = try OrlixRootfsTarImporter().importArchiveData(
            data,
            using: plan,
            registry: registry
        )

        XCTAssertEqual(result.descriptor.id, "alpine-import")
        XCTAssertEqual(result.manifest.map(\.path), ["etc", "etc/os-release"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        XCTAssertEqual(
            try registry.load(environmentID: "alpine-import"),
            result.descriptor
        )
        XCTAssertEqual(
            result.materializationPlan.stagingRootDirectory,
            result.stagingRootDirectory
        )
        XCTAssertEqual(
            result.materializationPlan.baseImageURL,
            result.storageLayout.baseImageURL
        )
        XCTAssertEqual(
            result.materializationPlan.stateImageURL,
            result.storageLayout.stateImageURL
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.materializationPlan.baseTreeDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: result.materializationPlan.stateTreeDirectory
                    .appendingPathComponent("upper").path
            )
        )
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: result.materializationPlan.stateTreeDirectory
                    .appendingPathComponent("work").path
            )
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/etc" uid 0"#)
        )
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/etc" mode 040755"#)
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/etc/os-release" mode 0100755"#
            )
        )
        let stateMetadataCommands = try String(
            contentsOf: result.materializationPlan.stateMetadataCommandsURL
        )
        XCTAssertTrue(
            stateMetadataCommands.contains("set_inode_field /upper mode 040755")
        )
        XCTAssertTrue(
            stateMetadataCommands.contains("set_inode_field /work mode 040755")
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.storageLayout.baseImageURL.path
            )
        )
    }

    func testRootfsTarImporterReadsArchiveFromImportPlanURL() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let archive = root.appendingPathComponent("alpine-rootfs.tar")
        try FileManager.default.createDirectory(
            at: root,
            withIntermediateDirectories: true
        )
        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: archive,
            environmentID: "alpine-file-import",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-file-import"
        )
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(
                path: "etc/os-release",
                payload: Data("ID=alpine-file-import\n".utf8)
            )
        ])
        try data.write(to: archive)

        let result = try OrlixRootfsTarImporter().importArchive(
            using: plan,
            registry: registry
        )

        XCTAssertEqual(result.descriptor.id, "alpine-file-import")
        XCTAssertEqual(result.manifest.map(\.path), ["etc", "etc/os-release"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine-file-import\n"
        )
        XCTAssertEqual(
            try registry.load(environmentID: "alpine-file-import"),
            result.descriptor
        )
        XCTAssertTrue(
            result.stagingRootDirectory.path.hasSuffix(
                "tmp/Orlix/imports/alpine-file-import/rootfs"
            )
        )
        XCTAssertTrue(
            result.storageLayout.rootDirectory.path.hasSuffix(
                "Application Support/Orlix/environments/alpine-file-import"
            )
        )
    }

    func testRootfsTarImporterRefusesExistingEnvironmentBeforeReadingArchive()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let existing = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.existing"
        )
        try registry.save(existing)
        let existingLayout = try registry.layout(forEnvironmentID: existing.id)
        try Data("existing-base-image".utf8).write(to: existingLayout.baseImageURL)
        try Data("existing-state-image".utf8).write(to: existingLayout.stateImageURL)
        let missingArchive = root.appendingPathComponent("missing-rootfs.tar")
        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: missingArchive,
            environmentID: existing.id,
            registry: registry,
            rootImageIdentifier: "orlix.env.imported"
        )

        XCTAssertThrowsError(
            try OrlixRootfsTarImporter().importArchive(
                using: plan,
                registry: registry
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixRootfsTarImportError,
                .destinationExists(existing.id)
            )
        }

        XCTAssertFalse(FileManager.default.fileExists(atPath: missingArchive.path))
        XCTAssertEqual(
            try registry.load(environmentID: existing.id),
            existing
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.baseImageURL),
            Data("existing-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.stateImageURL),
            Data("existing-state-image".utf8)
        )
    }

    func testRootfsTarImporterCleansFailedImportBeforeRetry() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: root.appendingPathComponent("broken-rootfs.tar"),
            environmentID: "broken-tar-import",
            registry: registry,
            rootImageIdentifier: "orlix.env.broken-tar-import"
        )
        let invalid = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(
                path: "etc/missing-hardlink",
                type: "1",
                linkName: "etc/does-not-exist"
            )
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarImporter().importArchiveData(
                invalid,
                using: plan,
                registry: registry
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(atPath: plan.storageLayout.rootDirectory.path)
        )

        let valid = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=retry\n".utf8))
        ])
        let retry = try OrlixRootfsTarImporter().importArchiveData(
            valid,
            using: plan,
            registry: registry
        )

        XCTAssertEqual(retry.descriptor.id, "broken-tar-import")
        XCTAssertEqual(
            try String(
                contentsOf: retry.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=retry\n"
        )
        XCTAssertEqual(
            try registry.load(environmentID: "broken-tar-import"),
            retry.descriptor
        )
    }

    func testRootfsTarImporterDoesNotOverwriteExistingEnvironment() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let existing = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.existing"
        )
        try registry.save(existing)
        let existingLayout = try registry.layout(forEnvironmentID: existing.id)
        try Data("existing-base-image".utf8).write(to: existingLayout.baseImageURL)
        try Data("existing-state-image".utf8).write(to: existingLayout.stateImageURL)
        let plan = try OrlixRootfsTarImportPlan.plan(
            archiveURL: root.appendingPathComponent("default-rootfs.tar"),
            environmentID: existing.id,
            registry: registry,
            rootImageIdentifier: "orlix.env.imported"
        )
        let data = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=imported\n".utf8))
        ])

        XCTAssertThrowsError(
            try OrlixRootfsTarImporter().importArchiveData(
                data,
                using: plan,
                registry: registry
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixRootfsTarImportError,
                .destinationExists(existing.id)
            )
        }

        XCTAssertEqual(
            try registry.load(environmentID: existing.id),
            existing
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.baseImageURL),
            Data("existing-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.stateImageURL),
            Data("existing-state-image".utf8)
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: existingLayout.importScratchDirectory
                    .appendingPathComponent("rootfs", isDirectory: true).path
            )
        )
    }

    func testOCIDigestSHA256MatchesKnownVector() {
        XCTAssertEqual(
            OrlixOCIDigest.sha256Hex(Data("abc".utf8)),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        )
    }

    func testOCIImageLayoutReaderSelectsLinuxArm64AndVerifiesBlobs() throws {
        let layout = try writeOCILayout()

        let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

        XCTAssertEqual(imported.platform, "linux/arm64")
        XCTAssertEqual(imported.manifestDigest, layout.manifestDigest)
        XCTAssertEqual(imported.configDigest, layout.configDigest)
        XCTAssertEqual(
            imported.layers,
            [
                OrlixOCIImageLayer(
                    digest: layout.layerDigest,
                    mediaType: "application/vnd.oci.image.layer.v1.tar",
                    size: UInt64(layout.layerData[0].count)
                )
            ]
        )
        XCTAssertEqual(imported.processDefaults.environment["PATH"], "/usr/bin:/bin")
        XCTAssertEqual(imported.processDefaults.environment["EMPTY"], "")
        XCTAssertEqual(imported.processDefaults.entrypoint, ["/bin/sh"])
        XCTAssertEqual(imported.processDefaults.command, ["-c", "echo hello"])
        XCTAssertEqual(imported.processDefaults.workingDirectory, "/")
XCTAssertEqual(imported.processDefaults.user, "0")
XCTAssertEqual(imported.labels, [:])
XCTAssertNil(imported.stopSignal)
XCTAssertEqual(imported.rootfsDiffIDs, [])
    }

func testOCIImageLayoutReaderPreservesImageConfigLabels() throws {
let layout = try writeOCILayout(labels: [
"org.opencontainers.image.title": "Alpine",
"org.opencontainers.image.version": "3.20"
])
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
XCTAssertEqual(
imported.labels["org.opencontainers.image.title"],
"Alpine"
)
XCTAssertEqual(
imported.labels["org.opencontainers.image.version"],
"3.20"
)
}

func testOCIImageLayoutReaderRejectsInvalidImageConfigLabel() throws {
let layout = try writeOCILayout(labels: [
"org.opencontainers.image.title": "Alpine\u{0}"
])
XCTAssertThrowsError(
try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidLabelEntry("org.opencontainers.image.title")
)
}
}

func testOCIImageLayoutReaderPreservesImageStopSignal() throws {
let layout = try writeOCILayout(stopSignal: "SIGUSR1")
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
XCTAssertEqual(imported.stopSignal, 10)
}

func testOCIImageLayoutReaderRejectsInvalidImageStopSignal() throws {
let layout = try writeOCILayout(stopSignal: "SIGDOESNOTEXIST")
XCTAssertThrowsError(
try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidStopSignal("SIGDOESNOTEXIST")
)
}
}

func testOCIImageLayoutReaderPreservesImageExposedPorts() throws {
let layout = try writeOCILayout(exposedPorts: [
"443/tcp": [:],
"53/udp": [:],
"8443/sctp": [:]
])
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
XCTAssertEqual(
imported.exposedPorts,
[
OrlixEnvironmentExposedPort(port: 443, proto: "tcp"),
OrlixEnvironmentExposedPort(port: 53, proto: "udp"),
OrlixEnvironmentExposedPort(port: 8443, proto: "sctp")
]
)
}

func testOCIImageLayoutReaderRejectsInvalidImageExposedPort() throws {
let layout = try writeOCILayout(exposedPorts: [
"0/tcp": [:]
])
XCTAssertThrowsError(
try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidExposedPort("0/tcp")
)
}
}

func testOCIImageLayoutReaderPreservesImageVolumes() throws {
let layout = try writeOCILayout(volumes: [
"/var/cache/orlix": [:],
"/data/": [:]
])
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
XCTAssertEqual(imported.imageVolumes, ["/data", "/var/cache/orlix"])
}

func testOCIImageLayoutReaderRejectsInvalidImageVolume() throws {
let layout = try writeOCILayout(volumes: [
"relative": [:]
])
XCTAssertThrowsError(
try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidImageVolume("relative")
)
}
}

func testOCIImageLayoutReaderPreservesImageHealthcheck() throws {
let layout = try writeOCILayout(healthcheck: [
"Test": ["CMD-SHELL", "curl -f http://127.0.0.1/health || exit 1"],
"Interval": 30_000_000_000,
"Timeout": 5_000_000_000,
"StartPeriod": 10_000_000_000,
"StartInterval": 1_000_000_000,
"Retries": 3,
])
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

XCTAssertEqual(
imported.healthcheck,
OrlixEnvironmentHealthcheck(
test: ["CMD-SHELL", "curl -f http://127.0.0.1/health || exit 1"],
intervalNanoseconds: 30_000_000_000,
timeoutNanoseconds: 5_000_000_000,
startPeriodNanoseconds: 10_000_000_000,
startIntervalNanoseconds: 1_000_000_000,
retries: 3
)
)
}

func testOCIImageLayoutReaderPreservesDisabledImageHealthcheck() throws {
let layout = try writeOCILayout(healthcheck: [
"Test": ["NONE"],
])
let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

XCTAssertEqual(
imported.healthcheck,
OrlixEnvironmentHealthcheck(test: ["NONE"])
)
}

func testOCIImageLayoutReaderRejectsInvalidImageHealthcheck() throws {
let invalidLayouts: [(OCILayoutFixture, OrlixOCIImageLayoutError)] = [
(
try writeOCILayout(healthcheck: ["Interval": -1]),
.invalidHealthcheckDuration(-1)
),
(
try writeOCILayout(healthcheck: ["Retries": -1]),
.invalidHealthcheckRetries(-1)
),
(
try writeOCILayout(healthcheck: ["Test": ["CMD", "bad\u{0}arg"]]),
.invalidCommandEntry("bad\u{0}arg")
),
]

for (layout, expectedError) in invalidLayouts {
XCTAssertThrowsError(
try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
) { error in
XCTAssertEqual(error as? OrlixOCIImageLayoutError, expectedError)
}
}
}

func testOCIImageLayoutReaderSelectsRequestedPlatformVariant() throws {
let layout = try writeOCILayout(platformVariant: "v8")

        let imported = try OrlixOCIImageLayoutReader().readLayout(
            at: layout.root,
            platform: "linux/arm64/v8"
        )

        XCTAssertEqual(imported.platform, "linux/arm64/v8")
        XCTAssertEqual(imported.manifestDigest, layout.manifestDigest)
    }

    func testOCIImageLayoutReaderDoesNotSelectVariantForPlainPlatform() throws {
        let layout = try writeOCILayout(platformVariant: "v8")

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .missingPlatform("linux/arm64")
            )
        }
    }

    func testOCIImageLayoutReaderRejectsDigestMismatch() throws {
        let layout = try writeOCILayout()
        let blobURL = layout.root
            .appendingPathComponent("blobs/sha256", isDirectory: true)
            .appendingPathComponent(layout.layerDigest.split(separator: ":").last.map(String.init)!)
        try Data("changed".utf8).write(to: blobURL)

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        )
    }

    func testOCIImageLayoutReaderRejectsManifestSizeMismatch() throws {
        let layout = try writeOCILayout(manifestSizeOverride: 1)

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            guard case let .sizeMismatch(digest, expected, actual) =
                    error as? OrlixOCIImageLayoutError
            else {
                XCTFail("expected size mismatch, got \(error)")
                return
            }
            XCTAssertEqual(digest, layout.manifestDigest)
            XCTAssertEqual(expected, 1)
            XCTAssertGreaterThan(actual, expected)
        }
    }

    func testOCIImageLayoutReaderRejectsConfigSizeMismatch() throws {
        let layout = try writeOCILayout(configSizeOverride: 1)

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            guard case let .sizeMismatch(digest, expected, actual) =
                    error as? OrlixOCIImageLayoutError
            else {
                XCTFail("expected size mismatch, got \(error)")
                return
            }
            XCTAssertEqual(digest, layout.configDigest)
            XCTAssertEqual(expected, 1)
            XCTAssertGreaterThan(actual, expected)
        }
    }

    func testOCIImageLayoutReaderRejectsLayerSizeMismatch() throws {
        let layout = try writeOCILayout(layerSizeOverrides: [1])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            guard case let .sizeMismatch(digest, expected, actual) =
                    error as? OrlixOCIImageLayoutError
            else {
                XCTFail("expected size mismatch, got \(error)")
                return
            }
            XCTAssertEqual(digest, layout.layerDigest)
            XCTAssertEqual(expected, 1)
            XCTAssertGreaterThan(actual, expected)
        }
    }

    func testOCIImageLayoutReaderAcceptsDockerSchema2DescriptorMediaTypes() throws {
        let layout = try writeOCILayout(
            manifestMediaType: "application/vnd.docker.distribution.manifest.v2+json",
            configMediaType: "application/vnd.docker.container.image.v1+json"
        )

        let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

        XCTAssertEqual(imported.manifestDigest, layout.manifestDigest)
        XCTAssertEqual(imported.configDigest, layout.configDigest)
        XCTAssertEqual(imported.layers.map(\.digest), [layout.layerDigest])
    }

    func testOCIImageLayoutReaderRejectsUnsupportedManifestMediaType() throws {
        let mediaType = "application/vnd.example.image.manifest.v1+json"
        let layout = try writeOCILayout(manifestMediaType: mediaType)

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .unsupportedManifestMediaType(mediaType)
            )
        }
    }

    func testOCIImageLayoutReaderRejectsUnsupportedConfigMediaType() throws {
        let mediaType = "application/vnd.example.image.config.v1+json"
        let layout = try writeOCILayout(configMediaType: mediaType)

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .unsupportedConfigMediaType(mediaType)
            )
        }
    }

    func testOCIImageLayoutReaderParsesRootfsDiffIDs() throws {
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let diffID = "sha256:\(OrlixOCIDigest.sha256Hex(layer))"
        let layout = try writeOCILayout(
            layerData: [layer],
            rootfsDiffIDs: [diffID]
        )

        let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

        XCTAssertEqual(imported.rootfsDiffIDs, [diffID])
    }

    func testOCIImageLayoutReaderAcceptsRootfsLayersTypeWithoutDiffIDs() throws {
        let layout = try writeOCILayout(
            rootfsType: "layers",
            rootfsDiffIDs: []
        )

        let imported = try OrlixOCIImageLayoutReader().readLayout(at: layout.root)

        XCTAssertEqual(imported.rootfsDiffIDs, [])
        XCTAssertEqual(imported.layers.map(\.digest), [layout.layerDigest])
    }

    func testOCIImageLayoutReaderRejectsUnsupportedRootfsType() throws {
        let layout = try writeOCILayout(
            rootfsType: "unsupported",
            rootfsDiffIDs: []
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .unsupportedRootfsType("unsupported")
            )
        }
    }

    func testOCIImageLayoutReaderRejectsRootfsDiffIDCountMismatch() throws {
        let layout = try writeOCILayout(
            rootfsDiffIDs: [
                "sha256:\(String(repeating: "0", count: 64))",
                "sha256:\(String(repeating: "1", count: 64))"
            ]
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(at: layout.root)
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .rootfsDiffIDCountMismatch(expected: 1, actual: 2)
            )
        }
    }

    func testOCIImageLayoutImporterRejectsRootfsDiffIDMismatch() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let expected = "sha256:\(String(repeating: "0", count: 64))"
        let actual = "sha256:\(OrlixOCIDigest.sha256Hex(layer))"
        let layout = try writeOCILayout(
            layerData: [layer],
            rootfsDiffIDs: [expected]
        )
        let storage = try registry.layout(forEnvironmentID: "alpine-bad-diffid")

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-bad-diffid",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-bad-diffid"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .rootfsDiffIDMismatch(
                    layerIndex: 0,
                    expected: expected,
                    actual: actual
                )
            )
        }
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: storage.importScratchDirectory
                    .appendingPathComponent("rootfs", isDirectory: true)
                    .path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(atPath: storage.rootDirectory.path)
        )

        let retryLayout = try writeOCILayout(
            layerData: [layer],
            rootfsDiffIDs: [actual]
        )
        let retry = try OrlixOCIImageLayoutImporter().importLayout(
            at: retryLayout.root,
            environmentID: "alpine-bad-diffid",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-bad-diffid"
        )
        XCTAssertEqual(retry.descriptor.id, "alpine-bad-diffid")
        XCTAssertEqual(
            try registry.load(environmentID: "alpine-bad-diffid"),
            retry.descriptor
        )
    }

    func testOCIImageLayoutImporterVerifiesGzipLayerRootfsDiffID() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let tarData = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [try gzip(tarData)],
            layerMediaTypes: ["application/vnd.oci.image.layer.v1.tar+gzip"],
            rootfsDiffIDs: ["sha256:\(OrlixOCIDigest.sha256Hex(tarData))"]
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-gzip-diffid",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-gzip-diffid"
        )

        XCTAssertEqual(result.image.rootfsDiffIDs, [
            "sha256:\(OrlixOCIDigest.sha256Hex(tarData))"
        ])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
    }

    func testOCIImageLayoutImporterBindsNumericUserAndGroup() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "1000:100"
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-numeric-user",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-numeric-user"
        )

        XCTAssertEqual(result.image.processDefaults.user, "1000:100")
        XCTAssertEqual(result.descriptor.defaultUserID, 1000)
        XCTAssertEqual(result.descriptor.defaultGroupID, 100)
    }

    func testOCIImageLayoutImporterResolvesNamedUserFromImportedPasswd() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(
                path: "etc/passwd",
                payload: Data("app:x:1000:100:app:/home/app:/bin/sh\n".utf8)
            )
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "app"
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-named-user",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-named-user"
        )

        XCTAssertEqual(result.image.processDefaults.user, "app")
        XCTAssertEqual(result.descriptor.defaultUserID, 1000)
        XCTAssertEqual(result.descriptor.defaultGroupID, 100)
    }

    func testOCIImageLayoutImporterResolvesNamedGroupFromImportedGroup() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(
                path: "etc/passwd",
                payload: Data("app:x:1000:100:app:/home/app:/bin/sh\n".utf8)
            ),
            TarFixtureEntry(
                path: "etc/group",
                payload: Data("staff:x:200:\n".utf8)
            )
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "app:staff"
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-named-group",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-named-group"
        )

        XCTAssertEqual(result.image.processDefaults.user, "app:staff")
        XCTAssertEqual(result.descriptor.defaultUserID, 1000)
        XCTAssertEqual(result.descriptor.defaultGroupID, 200)
    }

    func testOCIImageLayoutImporterResolvesNamedGroupForNumericUser() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(
                path: "etc/group",
                payload: Data("staff:x:200:\n".utf8)
            )
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "1000:staff"
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-numeric-user-named-group",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-numeric-user-named-group"
        )

        XCTAssertEqual(result.image.processDefaults.user, "1000:staff")
        XCTAssertEqual(result.descriptor.defaultUserID, 1000)
        XCTAssertEqual(result.descriptor.defaultGroupID, 200)
    }

    func testOCIImageLayoutImporterRejectsMissingNamedUser() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(
                path: "etc/passwd",
                payload: Data("app:x:1000:100:app:/home/app:/bin/sh\n".utf8)
            )
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "missing"
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-missing-user",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-missing-user"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .unsupportedUser("missing")
            )
        }
    }

    func testOCIImageLayoutImporterRejectsMissingNamedGroup() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(
                path: "etc/passwd",
                payload: Data("app:x:1000:100:app:/home/app:/bin/sh\n".utf8)
            ),
            TarFixtureEntry(
                path: "etc/group",
                payload: Data("staff:x:200:\n".utf8)
            )
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            user: "app:missing"
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-missing-group",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-missing-group"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .unsupportedUser("app:missing")
            )
        }
    }

    func testOCIImageLayoutImporterBindsAbsoluteWorkingDirectory() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "work", type: "5"),
            TarFixtureEntry(path: "work/project", type: "5")
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            workingDirectory: "/work/project"
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-working-dir",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-working-dir"
        )

        XCTAssertEqual(result.image.processDefaults.workingDirectory, "/work/project")
        XCTAssertEqual(result.descriptor.defaultWorkingDirectory, "/work/project")
    }

func testOCIImageLayoutImporterCreatesMissingWorkingDirectory() throws {
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(workingDirectory: "/work/project")

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-created-working-dir",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-created-working-dir"
)

XCTAssertEqual(result.descriptor.defaultWorkingDirectory, "/work/project")
XCTAssertTrue(
FileManager.default.fileExists(
atPath: result.stagingRootDirectory
.appendingPathComponent("work/project", isDirectory: true).path
)
)
XCTAssertTrue(result.manifest.map(\.path).contains("work"))
XCTAssertTrue(result.manifest.map(\.path).contains("work/project"))
}

func testOCIImageLayoutImporterRejectsWorkingDirectoryBlockedByFile()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layer = tarArchive(entries: [
TarFixtureEntry(path: "work", payload: Data("not-a-dir\n".utf8))
])
let layout = try writeOCILayout(
layerData: [layer],
workingDirectory: "/work/project"
)

XCTAssertThrowsError(
try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-blocked-working-dir",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-blocked-working-dir"
)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidWorkingDirectory("/work/project")
)
}
}

func testOCIImageLayoutImporterPreservesImageLabelsAsDescriptorAnnotations()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(labels: [
"org.opencontainers.image.ref.name": "docker.io/library/alpine:3.20",
"org.opencontainers.image.revision": "sha256:demo"
])

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-labels",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-labels"
)
let loaded = try registry.load(environmentID: "alpine-labels")

XCTAssertEqual(result.image.labels, loaded.annotations)
XCTAssertEqual(result.descriptor.annotations, loaded.annotations)
XCTAssertEqual(
loaded.annotations["org.opencontainers.image.ref.name"],
"docker.io/library/alpine:3.20"
)
XCTAssertEqual(
loaded.annotations["org.opencontainers.image.revision"],
"sha256:demo"
)
}

func testOCIImageLayoutImporterPreservesImageExposedPortsAsDescriptorMetadata()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(exposedPorts: [
"8080/tcp": [:],
"5353/udp": [:]
])

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-exposed-ports",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-exposed-ports"
)
let loaded = try registry.load(environmentID: "alpine-exposed-ports")
let expected = [
OrlixEnvironmentExposedPort(port: 5353, proto: "udp"),
OrlixEnvironmentExposedPort(port: 8080, proto: "tcp")
]

XCTAssertEqual(result.image.exposedPorts, expected)
XCTAssertEqual(result.descriptor.exposedPorts, expected)
XCTAssertEqual(loaded.exposedPorts, expected)
}

func testOCIImageLayoutImporterPreservesImageHealthcheckAsDescriptorMetadata()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(healthcheck: [
"Test": ["CMD", "/bin/check-health"],
"Interval": 15_000_000_000,
"Timeout": 2_000_000_000,
"Retries": 5,
])

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-healthcheck",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-healthcheck"
)
let loaded = try registry.load(environmentID: "alpine-healthcheck")
let expected = OrlixEnvironmentHealthcheck(
test: ["CMD", "/bin/check-health"],
intervalNanoseconds: 15_000_000_000,
timeoutNanoseconds: 2_000_000_000,
retries: 5
)

XCTAssertEqual(result.image.healthcheck, expected)
XCTAssertEqual(result.descriptor.healthcheck, expected)
XCTAssertEqual(loaded.healthcheck, expected)
}

func testOCIImageLayoutImporterMaterializesImageVolumes()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(volumes: [
"/data": [:],
"/var/cache/orlix": [:]
])

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-image-volumes",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-image-volumes"
)
let loaded = try registry.load(environmentID: "alpine-image-volumes")

XCTAssertEqual(result.image.imageVolumes, ["/data", "/var/cache/orlix"])
XCTAssertEqual(result.descriptor.imageVolumes, ["/data", "/var/cache/orlix"])
XCTAssertEqual(loaded.imageVolumes, ["/data", "/var/cache/orlix"])
XCTAssertTrue(
FileManager.default.fileExists(
atPath: result.stagingRootDirectory
.appendingPathComponent("data", isDirectory: true).path
)
)
XCTAssertTrue(
FileManager.default.fileExists(
atPath: result.stagingRootDirectory
.appendingPathComponent("var/cache/orlix", isDirectory: true).path
)
)
XCTAssertTrue(result.manifest.map(\.path).contains("data"))
XCTAssertTrue(result.manifest.map(\.path).contains("var"))
XCTAssertTrue(result.manifest.map(\.path).contains("var/cache"))
XCTAssertTrue(result.manifest.map(\.path).contains("var/cache/orlix"))
}

func testOCIImageLayoutImporterRejectsImageVolumeBlockedByFile()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layer = tarArchive(entries: [
TarFixtureEntry(path: "data", payload: Data("not-a-dir\n".utf8))
])
let layout = try writeOCILayout(layerData: [layer], volumes: [
"/data/cache": [:]
])

XCTAssertThrowsError(
try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-blocked-image-volume",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-blocked-image-volume"
)
) { error in
XCTAssertEqual(
error as? OrlixOCIImageLayoutError,
.invalidImageVolume("/data/cache")
)
}
}

func testOCIImageLayoutImporterPreservesImageStopSignalAsDescriptorDefault()
throws
{
let root = temporaryRegistryRoot()
let registry = OrlixEnvironmentRegistry(
linuxStateRoot: root.appendingPathComponent(
"Application Support/Orlix",
isDirectory: true
),
cacheRoot: root.appendingPathComponent(
"Caches/Orlix",
isDirectory: true
),
scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
)
let layout = try writeOCILayout(stopSignal: "SIGUSR2")

let result = try OrlixOCIImageLayoutImporter().importLayout(
at: layout.root,
environmentID: "alpine-stop-signal",
registry: registry,
rootImageIdentifier: "orlix.env.alpine-stop-signal"
)
let loaded = try registry.load(environmentID: "alpine-stop-signal")

XCTAssertEqual(result.image.stopSignal, 12)
XCTAssertEqual(result.descriptor.defaultStopSignal, 12)
XCTAssertEqual(loaded.defaultStopSignal, 12)
}

func testOCIImageLayoutImporterRejectsRelativeWorkingDirectory() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "work", type: "5")
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            workingDirectory: "work"
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-relative-working-dir",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-relative-working-dir"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidWorkingDirectory("work")
            )
        }
        XCTAssertThrowsError(
            try registry.load(environmentID: "alpine-relative-working-dir")
        )
    }

    func testOCIImageLayoutImporterBindsEnvironmentEntries() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            envEntries: [
                "PATH=/usr/local/bin:/usr/bin:/bin",
                "EMPTY=",
                "LANG=C.UTF-8"
            ]
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-env",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-env"
        )

        XCTAssertEqual(
            result.image.processDefaults.environment["PATH"],
            "/usr/local/bin:/usr/bin:/bin"
        )
        XCTAssertEqual(result.image.processDefaults.environment["EMPTY"], "")
        XCTAssertEqual(result.image.processDefaults.environment["LANG"], "C.UTF-8")
        XCTAssertEqual(
            result.descriptor.defaultEnvironment,
            result.image.processDefaults.environment
        )
    }

    func testOCIImageLayoutImporterRejectsMalformedEnvironmentEntry() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try writeOCILayout(envEntries: ["PATH=/usr/bin:/bin", "BAD"])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-bad-env",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-bad-env"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidEnvironmentEntry("BAD")
            )
        }
        XCTAssertThrowsError(try registry.load(environmentID: "alpine-bad-env"))
    }

	func testOCIImageLayoutImporterRejectsEmptyEnvironmentKey() throws {
		let root = temporaryRegistryRoot()
		let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try writeOCILayout(envEntries: ["=value"])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-empty-env-key",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-empty-env-key"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidEnvironmentEntry("=value")
            )
		}
	}

	func testOCIImageLayoutImporterRejectsDuplicateEnvironmentKey() throws {
		let root = temporaryRegistryRoot()
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: root.appendingPathComponent(
				"Application Support/Orlix",
				isDirectory: true
			),
			cacheRoot: root.appendingPathComponent(
				"Caches/Orlix",
				isDirectory: true
			),
			scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
		)
		let layout = try writeOCILayout(envEntries: ["PATH=/usr/bin", "PATH=/bin"])

		XCTAssertThrowsError(
			try OrlixOCIImageLayoutImporter().importLayout(
				at: layout.root,
				environmentID: "alpine-duplicate-env",
				registry: registry,
				rootImageIdentifier: "orlix.env.alpine-duplicate-env"
			)
		) { error in
			XCTAssertEqual(
				error as? OrlixOCIImageLayoutError,
				.invalidEnvironmentEntry("PATH=/bin")
			)
		}
	}

	func testOCIImageLayoutImporterRejectsNulEnvironmentEntry() throws {
		let root = temporaryRegistryRoot()
		let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try writeOCILayout(envEntries: ["BAD\u{0}KEY=value"])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-nul-env",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-nul-env"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidEnvironmentEntry("BAD\u{0}KEY=value")
            )
        }
    }

    func testOCIImageLayoutImporterBindsEntrypointAndCommand() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "bin", type: "5"),
            TarFixtureEntry(path: "bin/app", payload: Data("app\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            entrypoint: ["/bin/app"],
            command: ["--flag", "value"]
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-entrypoint-command",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-entrypoint-command"
        )

        XCTAssertEqual(result.image.processDefaults.entrypoint, ["/bin/app"])
        XCTAssertEqual(result.image.processDefaults.command, ["--flag", "value"])
        XCTAssertEqual(result.descriptor.defaultCommand, ["/bin/app", "--flag", "value"])
    }

    func testOCIImageLayoutImporterBindsCommandOnly() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "bin", type: "5"),
            TarFixtureEntry(path: "bin/echo", payload: Data("echo\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            entrypoint: [],
            command: ["/bin/echo", "hello"]
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-command-only",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-command-only"
        )

        XCTAssertEqual(result.image.processDefaults.entrypoint, [])
        XCTAssertEqual(result.image.processDefaults.command, ["/bin/echo", "hello"])
        XCTAssertEqual(result.descriptor.defaultCommand, ["/bin/echo", "hello"])
    }

    func testOCIImageLayoutImporterDefaultsEmptyCommandToShell() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "bin", type: "5"),
            TarFixtureEntry(path: "bin/sh", payload: Data("sh\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [layer],
            entrypoint: [],
            command: []
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-empty-command",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-empty-command"
        )

        XCTAssertEqual(result.image.processDefaults.entrypoint, [])
        XCTAssertEqual(result.image.processDefaults.command, [])
        XCTAssertEqual(result.descriptor.defaultCommand, ["/bin/sh"])
    }

    func testOCIImageLayoutImporterRejectsNulEntrypoint() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try writeOCILayout(entrypoint: ["/bin/sh\u{0}"], command: [])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-nul-entrypoint",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-nul-entrypoint"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidCommandEntry("/bin/sh\u{0}")
            )
        }
    }

    func testOCIImageLayoutImporterRejectsNulCommand() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try writeOCILayout(command: ["-c", "echo\u{0}bad"])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-nul-command",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-nul-command"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .invalidCommandEntry("echo\u{0}bad")
            )
        }
    }

    func testOCIImageLayoutReaderRejectsMissingPlatform() throws {
        let layout = try writeOCILayout()

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutReader().readLayout(
                at: layout.root,
                platform: "linux/amd64"
            )
        )
    }

    func testOCIImageLayoutImporterAppliesLayerWhiteoutsIntoStagingRoot() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8)),
            TarFixtureEntry(path: "etc/remove-me", payload: Data("lower\n".utf8)),
            TarFixtureEntry(path: "etc/opaque/", type: "5"),
            TarFixtureEntry(path: "etc/opaque/old", payload: Data("old\n".utf8))
        ])
        let upperLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/.wh.remove-me"),
            TarFixtureEntry(path: "etc/opaque/.wh..wh..opq"),
            TarFixtureEntry(path: "etc/opaque/new", payload: Data("new\n".utf8))
        ])
        let layout = try writeOCILayout(layerData: [lowerLayer, upperLayer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci"
        )

        XCTAssertEqual(result.descriptor.id, "alpine-oci")
        XCTAssertEqual(result.descriptor.source, .ociLayout)
        XCTAssertEqual(result.descriptor.defaultCommand, ["/bin/sh", "-c", "echo hello"])
        XCTAssertEqual(
            result.manifest.map(\.path),
            ["etc", "etc/os-release", "etc/opaque", "etc/opaque/new"]
        )
        XCTAssertEqual(
            try registry.load(environmentID: "alpine-oci"),
            result.descriptor
        )
        XCTAssertEqual(
            result.materializationPlan.stagingRootDirectory,
            result.stagingRootDirectory
        )
        XCTAssertEqual(
            result.materializationPlan.baseImageURL,
            result.storageLayout.baseImageURL
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("etc/remove-me").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("etc/.wh.remove-me").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("etc/opaque/old").path
            )
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/opaque/new")
            ),
            "new\n"
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.materializationPlan.baseTreeDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.materializationPlan.baseTreeDirectory
                    .appendingPathComponent("etc/remove-me").path
            )
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/etc" uid 0"#)
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/etc/os-release" mode 0100755"#
            )
        )
        XCTAssertFalse(metadataCommands.contains(#""/etc/remove-me""#))
        XCTAssertFalse(metadataCommands.contains(#""/etc/opaque/old""#))
        XCTAssertFalse(metadataCommands.contains(".wh.remove-me"))
        let stateMetadataCommands = try String(
            contentsOf: result.materializationPlan.stateMetadataCommandsURL
        )
        XCTAssertTrue(
            stateMetadataCommands.contains("set_inode_field /upper mode 040755")
        )
        XCTAssertTrue(
            stateMetadataCommands.contains("set_inode_field /work mode 040755")
        )
    }

    func testOCIImageLayoutImporterAppliesRootOpaqueWhiteoutToManifest() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/old", payload: Data("old\n".utf8)),
            TarFixtureEntry(path: "usr/", type: "5"),
            TarFixtureEntry(path: "usr/bin/", type: "5"),
            TarFixtureEntry(path: "usr/bin/tool", payload: Data("tool\n".utf8))
        ])
        let upperLayer = tarArchive(entries: [
            TarFixtureEntry(path: ".wh..wh..opq"),
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=reset\n".utf8))
        ])
        let layout = try writeOCILayout(layerData: [lowerLayer, upperLayer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-root-opaque",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-root-opaque"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["etc", "etc/os-release"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=reset\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("usr/bin/tool").path
            )
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertFalse(metadataCommands.contains(#""/etc/old""#))
        XCTAssertFalse(metadataCommands.contains(#""/usr""#))
        XCTAssertFalse(metadataCommands.contains(#""/usr/bin/tool""#))
    }

    func testOCIImageLayoutImporterRecordsImplicitOpaqueDirectoryInManifest()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(
                path: "var/cache/old",
                payload: Data("old\n".utf8)
            )
        ])
        let upperLayer = tarArchive(entries: [
            TarFixtureEntry(path: "var/cache/.wh..wh..opq"),
            TarFixtureEntry(
                path: "var/cache/new",
                payload: Data("new\n".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [lowerLayer, upperLayer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-implicit-opaque-dir",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-implicit-opaque-dir"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["var/cache", "var/cache/new"])
        XCTAssertEqual(result.manifest.first?.type, .directory)
        XCTAssertEqual(result.manifest.first?.mode, 0o755)
        XCTAssertEqual(result.manifest.first?.uid, 0)
        XCTAssertEqual(result.manifest.first?.gid, 0)
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("var/cache/old").path
            )
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("var/cache/new")
            ),
            "new\n"
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/var/cache" mode 040755"#)
        )
        XCTAssertFalse(metadataCommands.contains(#""/var/cache/old""#))
    }

    func testOCIImageLayoutImporterPreservesUstarPrefixPaths() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "os-release",
                prefix: "usr/lib",
                payload: Data("ID=orlix-prefix-proof\n".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-ustar-prefix",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-ustar-prefix"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["usr/lib/os-release"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("usr/lib/os-release")
            ),
            "ID=orlix-prefix-proof\n"
        )

        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/usr/lib/os-release" mode 0100755"#
            )
        )
    }

    func testOCIImageLayoutImporterPreservesChildrenWhenDirectoryEntryComesLater()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "etc/os-release",
                payload: Data("ID=late-dir\n".utf8)
            ),
            TarFixtureEntry(path: "etc/", type: "5")
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-late-dir-entry",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-late-dir-entry"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["etc/os-release", "etc"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=late-dir\n"
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/etc/os-release" mode 0100755"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/etc" mode 040755"#)
        )
    }

    func testOCIImageLayoutImporterReplacesLowerDirectoryTreeWithRegularFile()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(path: "var/", type: "5"),
            TarFixtureEntry(path: "var/lib/", type: "5"),
            TarFixtureEntry(path: "var/lib/app/", type: "5"),
            TarFixtureEntry(
                path: "var/lib/app/old",
                payload: Data("lower-old\n".utf8)
            ),
            TarFixtureEntry(path: "var/lib/app/nested/", type: "5"),
            TarFixtureEntry(
                path: "var/lib/app/nested/old",
                payload: Data("lower-nested-old\n".utf8)
            )
        ])
        let upperLayer = tarArchive(entries: [
            TarFixtureEntry(
                path: "var/lib/app",
                payload: Data("upper-file\n".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [lowerLayer, upperLayer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-directory-replaced-by-file",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-directory-replaced-by-file"
        )

        XCTAssertEqual(
            result.manifest.map(\.path),
            ["var", "var/lib", "var/lib/app"]
        )
        XCTAssertEqual(result.manifest.last?.type, .regularFile)
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("var/lib/app")
            ),
            "upper-file\n"
        )
        var isDirectory = ObjCBool(false)
        XCTAssertTrue(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("var/lib/app").path,
                isDirectory: &isDirectory
            )
        )
        XCTAssertFalse(isDirectory.boolValue)
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("var/lib/app/old").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("var/lib/app/nested/old").path
            )
        )

        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/var/lib/app" mode 0100755"#
            )
        )
        XCTAssertFalse(metadataCommands.contains("var/lib/app/old"))
        XCTAssertFalse(metadataCommands.contains("var/lib/app/nested"))
    }

    func testOCIImageLayoutImporterHonorsWhiteoutOrderWithinLayer()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(
                path: "etc/transient",
                payload: Data("transient\n".utf8)
            ),
            TarFixtureEntry(path: "etc/.wh.transient"),
            TarFixtureEntry(
                path: "etc/final",
                payload: Data("final\n".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-whiteout-order",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-whiteout-order"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["etc", "etc/final"])
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("etc/transient").path
            )
        )
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/final")
            ),
            "final\n"
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertFalse(metadataCommands.contains(#""/etc/transient""#))
        XCTAssertFalse(metadataCommands.contains(".wh.transient"))
        XCTAssertTrue(
            metadataCommands.contains(#"set_inode_field "/etc/final" mode 0100755"#)
        )
    }

    func testOCIImageLayoutImporterWhiteoutRemovesDanglingSymlink()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(
                path: "etc/dangling",
                type: "2",
                linkName: "/missing-target"
            )
        ])
        let upperLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/.wh.dangling")
        ])
        let layout = try writeOCILayout(layerData: [lowerLayer, upperLayer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-whiteout-dangling-symlink",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-whiteout-dangling-symlink"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["etc"])
        XCTAssertThrowsError(
            try FileManager.default.destinationOfSymbolicLink(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("etc/dangling").path
            )
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertFalse(metadataCommands.contains("dangling"))
        XCTAssertFalse(metadataCommands.contains(".wh.dangling"))
    }

    func testOCIImageLayoutImporterDoesNotOverwriteExistingEnvironment()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let existing = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.existing"
        )
        try registry.save(existing)
        let existingLayout = try registry.layout(forEnvironmentID: existing.id)
        try Data("existing-base-image".utf8).write(to: existingLayout.baseImageURL)
        try Data("existing-state-image".utf8).write(to: existingLayout.stateImageURL)
        let ociLayout = try writeOCILayout(layerData: [
            tarArchive(entries: [
                TarFixtureEntry(path: "etc/", type: "5"),
                TarFixtureEntry(
                    path: "etc/os-release",
                    payload: Data("ID=oci\n".utf8)
                )
            ])
        ])

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: ociLayout.root,
                environmentID: existing.id,
                registry: registry,
                rootImageIdentifier: "orlix.env.imported"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .destinationExists(existing.id)
            )
        }

        XCTAssertEqual(
            try registry.load(environmentID: existing.id),
            existing
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.baseImageURL),
            Data("existing-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.stateImageURL),
            Data("existing-state-image".utf8)
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: existingLayout.importScratchDirectory
                    .appendingPathComponent("rootfs", isDirectory: true).path
            )
        )
    }

    func testOCIImageLayoutImporterRefusesExistingEnvironmentBeforeReadingLayout()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let existing = OrlixEnvironmentDescriptor.defaultEnvironment(
            rootImageIdentifier: "orlix.env.existing"
        )
        try registry.save(existing)
        let existingLayout = try registry.layout(forEnvironmentID: existing.id)
        try Data("existing-base-image".utf8).write(to: existingLayout.baseImageURL)
        try Data("existing-state-image".utf8).write(to: existingLayout.stateImageURL)
        let missingLayout = root.appendingPathComponent(
            "missing-oci-layout",
            isDirectory: true
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: missingLayout,
                environmentID: existing.id,
                registry: registry,
                rootImageIdentifier: "orlix.env.imported"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixOCIImageLayoutError,
                .destinationExists(existing.id)
            )
        }

        XCTAssertFalse(FileManager.default.fileExists(atPath: missingLayout.path))
        XCTAssertEqual(try registry.load(environmentID: existing.id), existing)
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.baseImageURL),
            Data("existing-base-image".utf8)
        )
        XCTAssertEqual(
            try Data(contentsOf: existingLayout.stateImageURL),
            Data("existing-state-image".utf8)
        )
    }

    func testOCIImageLayoutImporterAppliesGzipLayerIntoStagingRoot() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let tarData = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=alpine\n".utf8))
        ])
        let layout = try writeOCILayout(
            layerData: [try gzip(tarData)],
            layerMediaTypes: ["application/vnd.oci.image.layer.v1.tar+gzip"]
        )

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-gzip-oci",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-gzip-oci"
        )

        XCTAssertEqual(result.descriptor.source, .ociLayout)
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("etc/os-release")
            ),
            "ID=alpine\n"
        )
    }

    func testOCIImageLayoutImporterAppliesPAXLayerMetadataIntoStagingRoot() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let paxPath = "usr/share/orlix/oci-pax-file"
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/oci-pax-file",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": paxPath,
                    "uid": "1000",
                    "gid": "100",
                    "mode": "0640",
                    "LIBARCHIVE.xattr.user.oci%2Ecomment": "b2NpIGxpYmFyY2hpdmU=",
                    "SCHILY.xattr.security.selinux": "system_u:object_r:usr_t:s0"
                ])
            ),
            TarFixtureEntry(path: "oci-pax-file", payload: Data("oci-pax\n".utf8)),
            TarFixtureEntry(
                path: "PaxHeaders.0/oci-tool",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "bin/oci-tool",
                    "linkpath": "/usr/bin/busybox",
                    "mode": "0777"
                ])
            ),
            TarFixtureEntry(path: "oci-tool", type: "2", linkName: "ignored")
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci-pax",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci-pax"
        )

        XCTAssertEqual(result.manifest.map(\.path), [paxPath, "bin/oci-tool"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent(paxPath)
            ),
            "oci-pax\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("oci-pax-file").path
            )
        )
        XCTAssertEqual(
            try FileManager.default.destinationOfSymbolicLink(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("bin/oci-tool").path
            ),
            "/usr/bin/busybox"
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/usr/share/orlix/oci-pax-file" uid 1000"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/usr/share/orlix/oci-pax-file" gid 100"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/usr/share/orlix/oci-pax-file" mode 0100640"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"ea_set "/usr/share/orlix/oci-pax-file" security.selinux "system_u:object_r:usr_t:s0""#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"ea_set "/usr/share/orlix/oci-pax-file" user.oci.comment "oci libarchive""#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/bin/oci-tool" mode 0120777"#
            )
        )
    }

    func testOCIImageLayoutImporterAppliesGNULongLayerNamesIntoStagingRoot() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let longPath = "usr/share/orlix/" + String(repeating: "oci-long/", count: 10)
            + "payload"
        let longLink = "/usr/bin/" + String(repeating: "busybox-", count: 12)
            + "target"
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "././@LongLink",
                type: "L",
                payload: gnuLongNamePayload(longPath)
            ),
            TarFixtureEntry(path: "payload", payload: Data("oci-gnu\n".utf8)),
            TarFixtureEntry(
                path: "././@LongLink",
                type: "K",
                payload: gnuLongNamePayload(longLink)
            ),
            TarFixtureEntry(path: "tool", type: "2", linkName: "ignored")
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci-gnu",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci-gnu"
        )

        XCTAssertEqual(result.manifest.map(\.path), [longPath, "tool"])
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent(longPath)
            ),
            "oci-gnu\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("payload").path
            )
        )
        XCTAssertEqual(
            try FileManager.default.destinationOfSymbolicLink(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("tool").path
            ),
            longLink
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/\#(longPath)" mode 0100755"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/tool" mode 0120755"#
            )
        )
    }

    func testOCIImageLayoutImporterCarriesBase256NumericMetadata() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "var/lib/orlix/base256",
                payload: Data("oci-base256\n".utf8),
                uid: 131_072,
                gid: 262_144,
                numericEncoding: .base256
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci-base256",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci-base256"
        )

        XCTAssertEqual(result.manifest.first?.uid, 131_072)
        XCTAssertEqual(result.manifest.first?.gid, 262_144)
        XCTAssertEqual(
            try String(
                contentsOf: result.stagingRootDirectory
                    .appendingPathComponent("var/lib/orlix/base256")
            ),
            "oci-base256\n"
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/var/lib/orlix/base256" uid 131072"#
            )
        )
        XCTAssertTrue(
            metadataCommands.contains(
                #"set_inode_field "/var/lib/orlix/base256" gid 262144"#
            )
        )
    }

    func testOCIImageLayoutImporterCarriesLinuxSpecialFilesAsImageMetadata() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "dev/null",
                type: "3",
                devMajor: 1,
                devMinor: 3
            ),
            TarFixtureEntry(path: "run/initctl", type: "6")
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci-special-files",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci-special-files"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["dev/null", "run/initctl"])
        XCTAssertEqual(result.manifest.first?.type, .characterDevice)
        XCTAssertEqual(result.manifest.first?.deviceMajor, 1)
        XCTAssertEqual(result.manifest.first?.deviceMinor, 3)
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("dev/null").path
            )
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: result.stagingRootDirectory
                    .appendingPathComponent("run/initctl").path
            )
        )
        let metadataCommands = try String(
            contentsOf: result.materializationPlan.baseMetadataCommandsURL
        )
        XCTAssertTrue(
            metadataCommands.contains("cd /dev\nmknod null c 1 3")
        )
        XCTAssertTrue(
            metadataCommands.contains("set_inode_field /dev/null mode 020755")
        )
        XCTAssertTrue(
            metadataCommands.contains("cd /run\nmknod initctl p")
        )
        XCTAssertTrue(
            metadataCommands.contains(
                "set_inode_field /run/initctl mode 010755"
            )
        )
    }

    func testOCIImageLayoutImporterPreservesHardLinksIntoMaterializationInput()
        throws
    {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "usr/bin/busybox",
                payload: Data("busybox\n".utf8)
            ),
            TarFixtureEntry(
                path: "bin/sh",
                type: "1",
                linkName: "usr/bin/busybox"
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-hardlink",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-hardlink"
        )

        XCTAssertEqual(result.manifest.map(\.path), ["usr/bin/busybox", "bin/sh"])
        XCTAssertEqual(result.manifest[0].type, .regularFile)
        XCTAssertEqual(result.manifest[1].type, .hardLink)
        XCTAssertEqual(result.manifest[1].linkName, "usr/bin/busybox")

        let stagingOriginal = try FileManager.default.attributesOfItem(
            atPath: result.stagingRootDirectory
                .appendingPathComponent("usr/bin/busybox").path
        )
        let stagingHardLink = try FileManager.default.attributesOfItem(
            atPath: result.stagingRootDirectory
                .appendingPathComponent("bin/sh").path
        )
        XCTAssertEqual(
            stagingOriginal[.systemFileNumber] as? NSNumber,
            stagingHardLink[.systemFileNumber] as? NSNumber
        )

        let baseTreeOriginal = try FileManager.default.attributesOfItem(
            atPath: result.materializationPlan.baseTreeDirectory
                .appendingPathComponent("usr/bin/busybox").path
        )
        let baseTreeHardLink = try FileManager.default.attributesOfItem(
            atPath: result.materializationPlan.baseTreeDirectory
                .appendingPathComponent("bin/sh").path
        )
        XCTAssertEqual(
            baseTreeOriginal[.systemFileNumber] as? NSNumber,
            baseTreeHardLink[.systemFileNumber] as? NSNumber
        )
    }

    func testOCIImageLayoutImporterAppliesGNUSparsePAXMap() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/oci-sparse",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "var/lib/orlix/oci-sparse",
                    "GNU.sparse.map": "0,3,10,6",
                    "GNU.sparse.size": "16"
                ])
            ),
            TarFixtureEntry(
                path: "oci-sparse",
                payload: Data("abcXYZ123".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])

        let result = try OrlixOCIImageLayoutImporter().importLayout(
            at: layout.root,
            environmentID: "alpine-oci-sparse",
            registry: registry,
            rootImageIdentifier: "orlix.env.alpine-oci-sparse"
        )
        let fileData = try Data(
            contentsOf: result.stagingRootDirectory
                .appendingPathComponent("var/lib/orlix/oci-sparse")
        )

        XCTAssertEqual(result.manifest.first?.path, "var/lib/orlix/oci-sparse")
        XCTAssertEqual(
            result.manifest.first?.sparseExtents,
            [
                OrlixRootfsTarSparseExtent(offset: 0, length: 3),
                OrlixRootfsTarSparseExtent(offset: 10, length: 6)
            ]
        )
        XCTAssertEqual(result.manifest.first?.logicalSize, 16)
        XCTAssertEqual(fileData.count, 16)
        XCTAssertEqual(Array(fileData[0..<3]), Array(Data("abc".utf8)))
        XCTAssertEqual(Array(fileData[3..<10]), Array(repeating: 0, count: 7))
        XCTAssertEqual(Array(fileData[10..<16]), Array(Data("XYZ123".utf8)))
    }

    func testOCIImageLayoutImporterRejectsGNUSparsePAXPayloadLengthMismatch() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layer = tarArchive(entries: [
            TarFixtureEntry(
                path: "PaxHeaders.0/oci-sparse",
                type: "x",
                payload: paxExtendedHeaderPayload([
                    "path": "var/lib/orlix/oci-sparse",
                    "GNU.sparse.map": "0,3",
                    "GNU.sparse.size": "3"
                ])
            ),
            TarFixtureEntry(
                path: "oci-sparse",
                payload: Data("abcd".utf8)
            )
        ])
        let layout = try writeOCILayout(layerData: [layer])
        let storage = try registry.layout(forEnvironmentID: "alpine-oci-bad-sparse")

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: layout.root,
                environmentID: "alpine-oci-bad-sparse",
                registry: registry,
                rootImageIdentifier: "orlix.env.alpine-oci-bad-sparse"
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixRootfsTarManifestError,
                .invalidPAXExtendedHeader
            )
        }
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: storage.importScratchDirectory
                    .appendingPathComponent("rootfs", isDirectory: true)
                    .path
            )
        )
    }

    func testOCIImageLayoutImporterDoesNotCommitPartialRootOnLayerFailure() throws {
        let root = temporaryRegistryRoot()
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: root.appendingPathComponent(
                "Application Support/Orlix",
                isDirectory: true
            ),
            cacheRoot: root.appendingPathComponent(
                "Caches/Orlix",
                isDirectory: true
            ),
            scratchRoot: root.appendingPathComponent("tmp/Orlix", isDirectory: true)
        )
        let layout = try registry.layout(forEnvironmentID: "broken-oci")
        let stagingRoot = layout.importScratchDirectory
            .appendingPathComponent("rootfs", isDirectory: true)
        try FileManager.default.createDirectory(
            at: stagingRoot.appendingPathComponent("etc", isDirectory: true),
            withIntermediateDirectories: true
        )
        try Data("ID=previous\n".utf8).write(
            to: stagingRoot.appendingPathComponent("etc/os-release")
        )

        let lowerLayer = tarArchive(entries: [
            TarFixtureEntry(path: "etc/", type: "5"),
            TarFixtureEntry(path: "etc/os-release", payload: Data("ID=new\n".utf8))
        ])
        let failingUpperLayer = tarArchive(entries: [
            TarFixtureEntry(
                path: "etc/missing-hardlink",
                type: "1",
                linkName: "etc/does-not-exist"
            )
        ])
        let ociLayout = try writeOCILayout(
            layerData: [lowerLayer, failingUpperLayer]
        )

        XCTAssertThrowsError(
            try OrlixOCIImageLayoutImporter().importLayout(
                at: ociLayout.root,
                environmentID: "broken-oci",
                registry: registry,
                rootImageIdentifier: "orlix.env.broken-oci"
            )
        )
        XCTAssertEqual(
            try String(
                contentsOf: stagingRoot.appendingPathComponent("etc/os-release")
            ),
            "ID=previous\n"
        )
        XCTAssertFalse(
            FileManager.default.fileExists(
                atPath: stagingRoot
                    .appendingPathComponent("etc/missing-hardlink")
                    .path
            )
        )
        let scratchContents = try FileManager.default.contentsOfDirectory(
            at: layout.importScratchDirectory,
            includingPropertiesForKeys: nil
        )
        XCTAssertFalse(
            scratchContents.contains {
                $0.lastPathComponent.hasPrefix(".rootfs.oci-import-")
            }
        )
    }

    func testOCILayerDecoderSupportsGzipMediaType() throws {
        let gzipHello = Data([
            0x1f, 0x8b, 0x08, 0x00, 0xca, 0xd5, 0x29, 0x6a,
            0x00, 0x03, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07,
            0x00, 0x86, 0xa6, 0x10, 0x36, 0x05, 0x00, 0x00,
            0x00
        ])

        let decoded = try OrlixOCILayerDecoder().decode(
            layerData: gzipHello,
            mediaType: "application/vnd.oci.image.layer.v1.tar+gzip"
        )

        XCTAssertEqual(decoded, Data("hello".utf8))
    }

    func testOCIImageLayoutImporterRejectsZstdLayerMediaTypesUntilDecoderExists() throws {
        let mediaTypes = [
            "application/vnd.oci.image.layer.v1.tar+zstd",
            "application/vnd.oci.image.layer.nondistributable.v1.tar+zstd",
            "application/vnd.docker.image.rootfs.diff.tar.zstd"
        ]

        for (index, mediaType) in mediaTypes.enumerated() {
            let layout = try writeOCILayout(
                layerData: [Data("not-zstd".utf8)],
                layerMediaTypes: [mediaType]
            )

            XCTAssertThrowsError(
                try OrlixOCIImageLayoutImporter().importLayout(
                    at: layout.root,
                    environmentID: "compressed-\(index)",
                    registry: OrlixEnvironmentRegistry(
                        linuxStateRoot: temporaryRegistryRoot(),
                        cacheRoot: temporaryRegistryRoot(),
                        scratchRoot: temporaryRegistryRoot()
                    ),
                    rootImageIdentifier: "orlix.env.compressed"
                )
            ) { error in
                XCTAssertEqual(
                    error as? OrlixOCIImageLayoutError,
                    .unsupportedLayerMediaType(mediaType)
                )
            }
        }
    }

    func testStoragePolicyKeepsLinuxStateCacheScratchAndDocumentsSeparate() throws {
        let policy = OrlixStoragePolicy.current
        let applicationSupport = try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: false
        )
        let caches = try FileManager.default.url(
            for: .cachesDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: false
        )
        let linuxStateDirectory = try policy.linuxStateDirectory()
        let cacheDirectory = try policy.cacheDirectory()
        let scratchDirectory = policy.scratchDirectory()

        XCTAssertEqual(
            linuxStateDirectory,
            applicationSupport.appendingPathComponent("Orlix", isDirectory: true)
        )
        XCTAssertEqual(
            cacheDirectory,
            caches.appendingPathComponent("Orlix", isDirectory: true)
        )
        XCTAssertEqual(
            scratchDirectory,
            FileManager.default.temporaryDirectory
                .appendingPathComponent("Orlix", isDirectory: true)
        )
        XCTAssertNotEqual(linuxStateDirectory, cacheDirectory)
        XCTAssertNotEqual(linuxStateDirectory, scratchDirectory)
        XCTAssertNotEqual(cacheDirectory, scratchDirectory)
        XCTAssertEqual(policy.linuxTemporaryFilesystemType, "tmpfs")
        XCTAssertEqual(policy.documentsMountPolicy, .explicitMountOnly)
    }

    func testRootfsSourcesKeepTmpLinuxOwnedAndDocumentsOutOfRootTruth() throws {
        let sourceRoot = try repositoryRoot()
        let initSource = try String(
            contentsOf: sourceRoot
                .appendingPathComponent("OrlixOS/Sources/init/init.c")
        )
        let rootfsMakefile = try String(
            contentsOf: sourceRoot
                .appendingPathComponent("OrlixOS/Sources/make/rootfs.mk")
        )

        XCTAssertTrue(initSource.contains(#"mount_if_needed("tmpfs", "/tmp""#))
        XCTAssertTrue(rootfsMakefile.contains(#""$$root_tree/tmp""#))
        XCTAssertTrue(rootfsMakefile.contains(#"chmod 1777 "$$root_tree/tmp""#))
        XCTAssertFalse(initSource.contains("Documents"))
        XCTAssertFalse(rootfsMakefile.contains("Documents"))
    }

    func testPayloadBundleIsResolvedFromOrlixOSTargetMetadata() throws {
        let payloadURL = try XCTUnwrap(OrlixOSPayload.bundleURL)
        let profile = try XCTUnwrap(OrlixOSPayload.selectedBootProfile)
        let kernelCommandLine = try XCTUnwrap(OrlixOSPayload.kernelCommandLine)

        XCTAssertTrue(FileManager.default.fileExists(atPath: payloadURL.path))
        XCTAssertTrue(profile == .release || profile == .development)
        XCTAssertTrue(kernelCommandLine.contains("console=ttyS0"))
        XCTAssertTrue(kernelCommandLine.contains("console=hvc0"))
    }

    func testRootImageDescriptorsComeFromOrlixOSTargetMetadata() throws {
        let productRootIdentifier = try XCTUnwrap(
            OrlixOSPayload.productRootImageIdentifier
        )
        let descriptors = OrlixOSPayload.rootImageDescriptors

        XCTAssertFalse(productRootIdentifier.isEmpty)
        XCTAssertTrue(
            descriptors.contains { $0.identifier == productRootIdentifier }
        )
        let upstreamTestDescriptors = descriptors.filter {
            $0.initrdBundleName != nil
        }
        XCTAssertFalse(upstreamTestDescriptors.isEmpty)
        for descriptor in upstreamTestDescriptors {
            XCTAssertFalse(descriptor.role.isEmpty)
            XCTAssertFalse(descriptor.identifier.isEmpty)
            let kernelCommandLine = try XCTUnwrap(descriptor.kernelCommandLine)
            XCTAssertTrue(kernelCommandLine.contains("rdinit=/init"))
            XCTAssertTrue(kernelCommandLine.contains("orlix.root=initramfs-only"))
            XCTAssertNotNil(descriptor.initrdBundleName)
            XCTAssertNotNil(descriptor.initrdBundleExtension)
            XCTAssertNotNil(descriptor.initrdResource)
        }
    }

    func testSingleTerminalSessionCarriesInputAndOutput() {
        let transport = RecordingTerminalTransport()
        let session = OrlixTerminalSession(transport: transport)
        let receivedOutput = DataRecorder()
        let output = session.attachOutput { data in
            receivedOutput.append(data)
        }

        session.send(Data("whoami\r".utf8))
        transport.emit(Data("root\r\n".utf8))

        withExtendedLifetime(output) {
            XCTAssertEqual(transport.sentInput, [Data("whoami\r".utf8)])
            XCTAssertEqual(receivedOutput.values, [Data("root\r\n".utf8)])
        }
    }

    func testCancelledTerminalOutputStopsReceivingBytes() {
        let transport = RecordingTerminalTransport()
        let session = OrlixTerminalSession(transport: transport)
        let receivedOutput = DataRecorder()
        let output = session.attachOutput { data in
            receivedOutput.append(data)
        }

        output.cancel()
        transport.emit(Data("late output\r\n".utf8))

        XCTAssertTrue(receivedOutput.values.isEmpty)
    }

    private func repositoryRoot() throws -> URL {
        var url = URL(fileURLWithPath: #filePath)
        while url.path != "/" {
            let candidate = url.deletingLastPathComponent()
            if FileManager.default.fileExists(
                atPath: candidate.appendingPathComponent("project.yml").path
            ) {
                return candidate
            }
            url = candidate
        }
        throw NSError(
            domain: "OrlixOSTests",
            code: 1,
            userInfo: [NSLocalizedDescriptionKey: "could not locate repository root"]
        )
    }

    private func temporaryRegistryRoot() -> URL {
        let root = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        addTeardownBlock {
            try? FileManager.default.removeItem(at: root)
        }
        return root
    }

    private struct TarFixtureEntry {
        var path: String
        var prefix: String = ""
        var type: Unicode.Scalar = "0"
        var linkName: String = ""
        var payload: Data = Data()
        var sizeEncoding: TarNumericEncoding = .octal
        var uid: UInt64 = 0
        var gid: UInt64 = 0
        var modificationTime: UInt64 = 0
        var devMajor: UInt64 = 0
        var devMinor: UInt64 = 0
        var numericEncoding: TarNumericEncoding = .octal
    }

    private enum TarNumericEncoding {
        case octal
        case base256
    }

    private func tarArchive(entries: [TarFixtureEntry]) -> Data {
        var data = Data()
        for entry in entries {
            var header = Data(repeating: 0, count: 512)
            write(entry.path, into: &header, at: 0, length: 100)
            writeNumeric(
                0o755,
                encoding: entry.numericEncoding,
                into: &header,
                at: 100,
                length: 8
            )
            writeNumeric(
                entry.uid,
                encoding: entry.numericEncoding,
                into: &header,
                at: 108,
                length: 8
            )
            writeNumeric(
                entry.gid,
                encoding: entry.numericEncoding,
                into: &header,
                at: 116,
                length: 8
            )
            writeNumeric(
                UInt64(entry.payload.count),
                encoding: entry.sizeEncoding,
                into: &header,
                at: 124,
                length: 12
            )
            writeNumeric(
                entry.modificationTime,
                encoding: entry.numericEncoding,
                into: &header,
                at: 136,
                length: 12
            )
            for index in 148..<156 {
                header[index] = UInt8(ascii: " ")
            }
            header[156] = UInt8(ascii: entry.type)
            write(entry.linkName, into: &header, at: 157, length: 100)
            write("ustar", into: &header, at: 257, length: 6)
            write("00", into: &header, at: 263, length: 2)
            writeNumeric(
                entry.devMajor,
                encoding: entry.numericEncoding,
                into: &header,
                at: 329,
                length: 8
            )
            writeNumeric(
                entry.devMinor,
                encoding: entry.numericEncoding,
                into: &header,
                at: 337,
                length: 8
            )
            write(entry.prefix, into: &header, at: 345, length: 155)
            let checksum = header.reduce(UInt64(0)) { $0 + UInt64($1) }
            writeOctal(checksum, into: &header, at: 148, length: 8)
            data.append(header)
            data.append(entry.payload)
            let remainder = entry.payload.count % 512
            if remainder != 0 {
                data.append(Data(repeating: 0, count: 512 - remainder))
            }
        }
        data.append(Data(repeating: 0, count: 1024))
        return data
    }

    private func paxExtendedHeaderPayload(_ fields: [String: String]) -> Data {
        Data(
            fields.keys.sorted().map { key in
                paxExtendedHeaderRecord(key: key, value: fields[key]!)
            }.joined().utf8
        )
    }

    private func paxExtendedHeaderRecord(key: String, value: String) -> String {
        let body = "\(key)=\(value)\n"
        var length = body.utf8.count + 2
        while true {
            let record = "\(length) \(body)"
            let actualLength = record.utf8.count
            if actualLength == length {
                return record
            }
            length = actualLength
        }
    }

    private func gnuLongNamePayload(_ value: String) -> Data {
        Data((value + "\0").utf8)
    }

    private func writeNumeric(
        _ value: UInt64,
        encoding: TarNumericEncoding,
        into data: inout Data,
        at offset: Int,
        length: Int
    ) {
        switch encoding {
        case .octal:
            writeOctal(value, into: &data, at: offset, length: length)
        case .base256:
            writeBase256(value, into: &data, at: offset, length: length)
        }
    }

    private func write(
        _ string: String,
        into data: inout Data,
        at offset: Int,
        length: Int
    ) {
        let bytes = Array(string.utf8.prefix(length))
        for index in 0..<bytes.count {
            data[offset + index] = bytes[index]
        }
    }

    private func writeOctal(
        _ value: UInt64,
        into data: inout Data,
        at offset: Int,
        length: Int
    ) {
        let text = String(value, radix: 8)
        let padded = String(repeating: "0", count: max(0, length - 2 - text.count))
            + text
            + "\0"
        write(padded, into: &data, at: offset, length: length)
    }

    private func writeBase256(
        _ value: UInt64,
        into data: inout Data,
        at offset: Int,
        length: Int
    ) {
        var value = value
        for index in stride(from: offset + length - 1, through: offset, by: -1) {
            data[index] = UInt8(value & 0xff)
            value >>= 8
        }
        data[offset] |= 0x80
    }

    private struct OCILayoutFixture {
        let root: URL
        let manifestDigest: String
        let configDigest: String
        let layerDigests: [String]
        let layerData: [Data]

        var layerDigest: String {
            layerDigests[0]
        }
    }

    private func writeOCILayout(
        layerData: [Data] = [Data("layer-bytes".utf8)],
        layerMediaTypes: [String]? = nil,
        manifestSizeOverride: Int? = nil,
        configSizeOverride: Int? = nil,
        layerSizeOverrides: [Int]? = nil,
        rootfsType: String? = nil,
        rootfsDiffIDs: [String]? = nil,
        manifestMediaType: String = "application/vnd.oci.image.manifest.v1+json",
        configMediaType: String = "application/vnd.oci.image.config.v1+json",
        platformVariant: String? = nil,
        user: String = "0",
    workingDirectory: String = "/",
	envEntries: [String] = ["PATH=/usr/bin:/bin", "EMPTY="],
	entrypoint: [String] = ["/bin/sh"],
	command: [String] = ["-c", "echo hello"],
	labels: [String: String] = [:],
	stopSignal: String? = nil,
	exposedPorts: [String: [String: String]] = [:],
	volumes: [String: [String: String]] = [:],
	healthcheck: [String: Any]? = nil
) throws -> OCILayoutFixture {
        let root = temporaryRegistryRoot()
        let blobs = root.appendingPathComponent("blobs/sha256", isDirectory: true)
        try FileManager.default.createDirectory(
            at: blobs,
            withIntermediateDirectories: true
        )
        try #"{"imageLayoutVersion":"1.0.0"}"#
            .data(using: .utf8)!
            .write(to: root.appendingPathComponent("oci-layout"))

        let rootfsBlock: String
        if rootfsType != nil || rootfsDiffIDs != nil {
            let diffIDs = rootfsDiffIDs
                ?? layerData.map { "sha256:\(OrlixOCIDigest.sha256Hex($0))" }
            let encodedDiffIDs = diffIDs
                .map { #""\#($0)""# }
                .joined(separator: ", ")
            rootfsBlock = #"""
            ,
              "rootfs": {
                "type": "\#(rootfsType ?? "layers")",
                "diff_ids": [\#(encodedDiffIDs)]
              }
            """#
        } else {
            rootfsBlock = ""
        }
var configObject: [String: Any] = [
"config": [
"Env": envEntries,
"Entrypoint": entrypoint,
"Cmd": command,
"WorkingDir": workingDirectory,
"User": user,
"Labels": labels
]
]
if let stopSignal {
var config = configObject["config"] as! [String: Any]
config["StopSignal"] = stopSignal
configObject["config"] = config
}
if !exposedPorts.isEmpty {
var config = configObject["config"] as! [String: Any]
config["ExposedPorts"] = exposedPorts
configObject["config"] = config
}
	if !volumes.isEmpty {
		var config = configObject["config"] as! [String: Any]
		config["Volumes"] = volumes
		configObject["config"] = config
	}
	if let healthcheck {
		var config = configObject["config"] as! [String: Any]
		config["Healthcheck"] = healthcheck
		configObject["config"] = config
	}
	if rootfsType != nil || rootfsDiffIDs != nil {
let diffIDs = rootfsDiffIDs
?? layerData.map { "sha256:\(OrlixOCIDigest.sha256Hex($0))" }
configObject["rootfs"] = [
"type": rootfsType ?? "layers",
"diff_ids": diffIDs
]
}
let configData = try JSONSerialization.data(
withJSONObject: configObject,
options: [.sortedKeys]
)
        let configDigest = try writeOCIBlob(configData, under: blobs)
        let layerDigests = try layerData.map { try writeOCIBlob($0, under: blobs) }
        let mediaTypes = layerMediaTypes ?? Array(
            repeating: "application/vnd.oci.image.layer.v1.tar",
            count: layerData.count
        )
        let layerDescriptors = zip(zip(layerDigests, layerData), mediaTypes)
            .enumerated()
            .map { index, element in
                let (digestAndData, mediaType) = element
                let (digest, data) = digestAndData
                let advertisedSize = layerSizeOverrides?[index] ?? data.count
                return #"""
                    {
                      "mediaType": "\#(mediaType)",
                      "digest": "\#(digest)",
                      "size": \#(advertisedSize)
                    }
                """#
            }
            .joined(separator: ",\n")
        let advertisedConfigSize = configSizeOverride ?? configData.count
        let manifestData = #"""
        {
          "schemaVersion": 2,
          "config": {
            "mediaType": "\#(configMediaType)",
            "digest": "\#(configDigest)",
            "size": \#(advertisedConfigSize)
          },
          "layers": [
        \#(layerDescriptors)
          ]
        }
        """#.data(using: .utf8)!
        let manifestDigest = try writeOCIBlob(manifestData, under: blobs)
        let advertisedManifestSize = manifestSizeOverride ?? manifestData.count
        let platformVariantLine = platformVariant.map {
            ",\n                \"variant\": \"\($0)\""
        } ?? ""
        let indexData = #"""
        {
          "schemaVersion": 2,
          "manifests": [
            {
              "mediaType": "\#(manifestMediaType)",
              "digest": "\#(manifestDigest)",
              "size": \#(advertisedManifestSize),
              "platform": {
                "os": "linux",
                "architecture": "arm64"\#(platformVariantLine)
              }
            }
          ]
        }
        """#.data(using: .utf8)!
        try indexData.write(to: root.appendingPathComponent("index.json"))

        return OCILayoutFixture(
            root: root,
            manifestDigest: manifestDigest,
            configDigest: configDigest,
            layerDigests: layerDigests,
            layerData: layerData
        )
    }

    private func writeOCIBlob(_ data: Data, under blobs: URL) throws -> String {
        let hex = OrlixOCIDigest.sha256Hex(data)
        try data.write(to: blobs.appendingPathComponent(hex))
        return "sha256:\(hex)"
    }

    private func gzip(_ data: Data) throws -> Data {
        var stream = z_stream()
        let initStatus = deflateInit2_(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            MAX_WBITS + 16,
            8,
            Z_DEFAULT_STRATEGY,
            ZLIB_VERSION,
            Int32(MemoryLayout<z_stream>.size)
        )
        XCTAssertEqual(initStatus, Z_OK)
        guard initStatus == Z_OK else {
            throw NSError(domain: "OrlixOSTests.gzip", code: Int(initStatus))
        }
        defer {
            deflateEnd(&stream)
        }

        var output = Data()
        var input = Array(data)
        let chunkSize = 64 * 1024
        let status = input.withUnsafeMutableBytes { inputBuffer -> Int32 in
            guard let inputBase = inputBuffer.baseAddress else {
                return Z_DATA_ERROR
            }
            stream.next_in = inputBase.assumingMemoryBound(to: Bytef.self)
            stream.avail_in = uInt(inputBuffer.count)

            var status = Z_OK
            while status != Z_STREAM_END {
                var chunk = Array(repeating: UInt8(0), count: chunkSize)
                status = chunk.withUnsafeMutableBytes { outputBuffer -> Int32 in
                    stream.next_out = outputBuffer.baseAddress!
                        .assumingMemoryBound(to: Bytef.self)
                    stream.avail_out = uInt(outputBuffer.count)
                    return deflate(&stream, Z_FINISH)
                }
                let produced = chunkSize - Int(stream.avail_out)
                if produced > 0 {
                    output.append(chunk, count: produced)
                }
                if status == Z_STREAM_END {
                    break
                }
                if status != Z_OK {
                    return status
                }
                if produced == 0 {
                    return Z_BUF_ERROR
                }
            }
            return status
        }
        XCTAssertEqual(status, Z_STREAM_END)
        guard status == Z_STREAM_END else {
            throw NSError(domain: "OrlixOSTests.gzip", code: Int(status))
        }
        return output
    }

    func testRootfsManifestXattrsLowerToHostDirectoryMetadata() {
        let manifest = [
            OrlixRootfsTarManifestEntry(
                path: "./usr/bin/tool",
                size: 4,
                mode: 0o100755,
                uid: 0,
                gid: 0,
                type: .regularFile,
                linkName: nil,
                extendedAttributes: [
                    "user.comment": "hello",
                    "security.capability": "cap"
                ]
            )
        ]

        let lowered = OrlixHostDirectoryExtendedAttribute.lowering(
            identifier: "orlix-host0",
            manifest: manifest
        )

        XCTAssertEqual(
            lowered,
            [
                OrlixHostDirectoryExtendedAttribute(
                    identifier: "orlix-host0",
                    relativePath: "usr/bin/tool",
                    name: "security.capability",
                    value: Data("cap".utf8)
                ),
                OrlixHostDirectoryExtendedAttribute(
                    identifier: "orlix-host0",
                    relativePath: "usr/bin/tool",
                    name: "user.comment",
                    value: Data("hello".utf8)
                )
            ].compactMap { $0 }
        )
    }

    func testRootfsManifestXattrLoweringRejectsHostAndInvalidMetadata() {
        let manifest = [
            OrlixRootfsTarManifestEntry(
                path: "/absolute",
                size: 0,
                mode: 0o100644,
                uid: 0,
                gid: 0,
                type: .regularFile,
                linkName: nil,
                extendedAttributes: ["user.comment": "absolute"]
            ),
            OrlixRootfsTarManifestEntry(
                path: "var/../escape",
                size: 0,
                mode: 0o100644,
                uid: 0,
                gid: 0,
                type: .regularFile,
                linkName: nil,
                extendedAttributes: ["security.capability": "escape"]
            ),
            OrlixRootfsTarManifestEntry(
                path: "etc/config",
                size: 0,
                mode: 0o100644,
                uid: 0,
                gid: 0,
                type: .regularFile,
                linkName: nil,
                extendedAttributes: [
                    "com.apple.quarantine": "host",
                    "linux.invalid": "not-a-linux-xattr",
                    "trusted.overlay.opaque": "y"
                ]
            )
        ]

        let lowered = OrlixHostDirectoryExtendedAttribute.lowering(
            identifier: "orlix-host0",
            manifest: manifest
        )

        XCTAssertEqual(
            lowered,
            [
                OrlixHostDirectoryExtendedAttribute(
                    identifier: "orlix-host0",
                    relativePath: "etc/config",
                    name: "trusted.overlay.opaque",
                    value: Data("y".utf8)
                )
            ].compactMap { $0 }
        )
    }
}

extension OrlixTerminalSessionTests {
	func testOCIRuntimeFeatureReportDoesNotOverclaimBroadLinuxFeatures() throws {
		let report = OrlixOCIRuntimeFeatureReport.current

		XCTAssertEqual(report.schemaVersion, 1)
		XCTAssertEqual(report.platform, "linux/arm64")
		XCTAssertEqual(report.feature(named: "cgroups")?.status, .recognized)
		XCTAssertEqual(
			report.feature(named: "cgroupV2BasicLifecycle")?.status,
			.implemented
		)
		XCTAssertEqual(report.feature(named: "root.readonly")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "root.readonly")?.proof,
			"orlix:readonly_root_probe"
		)
		XCTAssertEqual(report.feature(named: "rootfsPropagation")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "rootfsPropagation")?.proof,
            "orlix:rootinit_mount_propagation"
        )
        XCTAssertEqual(report.feature(named: "maskedPaths")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "maskedPaths")?.proof,
            "orlix:rootinit_masked_paths"
        )
        XCTAssertEqual(report.feature(named: "readonlyPaths")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "readonlyPaths")?.proof,
            "orlix:rootinit_readonly_paths"
        )
        XCTAssertEqual(report.feature(named: "sysctl")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "sysctl")?.proof,
            "orlix:rootinit_procfs_sysctl"
        )
        XCTAssertEqual(report.feature(named: "seccomp")?.status, .deterministicallyRejected)
		XCTAssertEqual(report.feature(named: "intelRdt")?.status, .deterministicallyRejected)
        XCTAssertEqual(report.feature(named: "ociCgroupPath")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "ociCgroupPath")?.proof,
            "orlix:init_cgroup_path_join"
        )
        XCTAssertEqual(report.feature(named: "ociHugepageLimits")?.status, .deterministicallyRejected)
        XCTAssertEqual(report.feature(named: "ociLinuxDevices")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "ociLinuxDevices")?.proof,
            "orlix:device_node_probe"
        )
		XCTAssertEqual(report.feature(named: "ociLinuxResources")?.status, .recognized)
XCTAssertEqual(report.feature(named: "ociCPUQuota")?.status, .implemented)
XCTAssertEqual(
report.feature(named: "ociCPUQuota")?.proof,
"orlix:cgroup_cpu_probe"
)
XCTAssertEqual(report.feature(named: "ociCPUShares")?.status, .implemented)
XCTAssertEqual(
report.feature(named: "ociCPUShares")?.proof,
"orlix:cgroup_cpu_probe"
)
        XCTAssertEqual(report.feature(named: "ociPidsLimit")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "ociPidsLimit")?.proof,
            "orlix:cgroup_pids_probe"
        )
		XCTAssertEqual(report.feature(named: "ociPersonality")?.status, .recognized)
		XCTAssertNil(report.feature(named: "ociPersonality")?.proof)
        XCTAssertEqual(report.feature(named: "ociTimeNamespace")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "ociTimeNamespace")?.proof,
            "orlix:time_namespace_probe"
        )
		XCTAssertEqual(report.feature(named: "ociTimeOffsets")?.status, .recognized)
		XCTAssertNil(report.feature(named: "ociTimeOffsets")?.proof)
        XCTAssertEqual(report.feature(named: "ociMemoryLimit")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "ociMemoryLimit")?.proof,
			"orlix:cgroup_memory_probe"
		)
XCTAssertEqual(report.feature(named: "ociBlockIOControls")?.status, .implemented)
		XCTAssertEqual(
report.feature(named: "ociBlockIOControls")?.proof,
			"orlix:cgroup_io_probe"
		)
		XCTAssertEqual(report.feature(named: "ociMaskedPaths")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "ociMaskedPaths")?.proof,
			"orlix:rootinit_masked_paths"
		)
        XCTAssertEqual(report.feature(named: "ociNamespaces")?.status, .recognized)
        XCTAssertEqual(
            report.feature(named: "ociMountIpcUtsNetworkCgroupPidNamespaces")?.status,
            .implemented
        )
        XCTAssertEqual(
            report.feature(named: "ociMountIpcUtsNetworkCgroupPidNamespaces")?.proof,
            "orlix:mount_namespace_probe,orlix:ipc_namespace_probe,orlix:network_namespace_probe,orlix:cgroup_namespace_probe,orlix:namespace_probe"
        )
		XCTAssertEqual(report.feature(named: "ociNamespacePathJoins")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "ociNamespacePathJoins")?.proof,
			"orlix:runtime_config_parser"
		)
		XCTAssertEqual(report.feature(named: "ociReadonlyPaths")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "ociReadonlyPaths")?.proof,
			"orlix:rootinit_readonly_paths"
		)
		XCTAssertEqual(report.feature(named: "ociCgroupMounts")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "ociCgroupMounts")?.proof,
			"orlix:cgroup_v2_probe"
		)
        XCTAssertEqual(report.feature(named: "ociUnifiedCgroupResources")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "ociUnifiedCgroupResources")?.proof,
            "orlix:cgroup_unified_probe"
        )
        XCTAssertEqual(report.feature(named: "userNamespaceMappings")?.status, .implemented)
        XCTAssertEqual(
            report.feature(named: "userNamespaceMappings")?.proof,
            "orlix:user_namespace_probe"
        )
		XCTAssertEqual(report.feature(named: "idmappedMounts")?.status, .deterministicallyRejected)
		XCTAssertEqual(report.feature(named: "apparmor")?.status, .deterministicallyRejected)
		XCTAssertEqual(report.feature(named: "apparmor")?.proof, "orlix:runtime_config_parser")
		XCTAssertEqual(report.feature(named: "selinux")?.status, .deterministicallyRejected)
		for hookFeature in [
			"hooks.prestart",
			"hooks.createRuntime",
			"hooks.createContainer",
			"hooks.startContainer",
			"hooks.poststart",
			"hooks.poststop"
		] {
			XCTAssertEqual(report.feature(named: hookFeature)?.status, .deterministicallyRejected)
		}
		XCTAssertEqual(report.feature(named: "selinux")?.proof, "orlix:runtime_config_parser")
	XCTAssertEqual(report.feature(named: "ociBindMounts")?.status, .implemented)
	XCTAssertEqual(
		report.feature(named: "ociBindMounts")?.proof,
		"orlix:virtio_fs_mount_probe"
	)
		XCTAssertEqual(report.feature(named: "ociCgroupMounts")?.status, .implemented)
		XCTAssertEqual(
			report.feature(named: "cgroupV2PidsController")?.status,
			.implemented
		)
		XCTAssertEqual(
			report.feature(named: "cgroupV2PidsController")?.proof,
			"orlix:cgroup_pids_probe"
		)
		XCTAssertEqual(
			report.feature(named: "process.capabilities")?.proof,
			"orlix:process_capability_probe"
		)
		XCTAssertEqual(report.feature(named: "netDevices")?.status, .deterministicallyRejected)
		XCTAssertEqual(report.feature(named: "netDevices")?.proof, "orlix:runtime_config_parser")
		XCTAssertEqual(report.feature(named: "virtioNetDevicePlane")?.status, .implemented)
		XCTAssertEqual(report.feature(named: "virtioNetDevicePlane")?.proof, "orlix:virtio_net_device_probe")
		XCTAssertEqual(report.feature(named: "virtioFsHostFolderMount")?.status, .implemented)
		XCTAssertEqual(report.feature(named: "virtioFsHostFolderMount")?.proof, "orlix:virtio_fs_mount_probe")
		XCTAssertEqual(report.feature(named: "ociLifecycleStateModel")?.status, .recognized)
		XCTAssertNil(report.feature(named: "ociLifecycleStateModel")?.proof)
		XCTAssertEqual(report.feature(named: "ociRuntimeSpecLifecycle")?.status, .recognized)
		XCTAssertNil(report.feature(named: "ociRuntimeSpecLifecycle")?.proof)
		XCTAssertEqual(report.feature(named: "idmappedMounts")?.status, .deterministicallyRejected)
        XCTAssertEqual(
            report.feature(named: "userNamespaceMappings")?.status,
            .implemented
        )
        XCTAssertEqual(
            report.feature(named: "userNamespaceMappings")?.proof,
            "orlix:user_namespace_probe"
        )
    }

	func testOCIRuntimeFeatureReportDoesNotTreatProcessMetadataAsProof() throws {
		let report = OrlixOCIRuntimeFeatureReport.current
		let features = Dictionary(uniqueKeysWithValues: report.features.map { ($0.name, $0) })

		XCTAssertEqual(features["process.capabilities"]?.status, .implemented)
		XCTAssertEqual(
			features["process.capabilities"]?.proof,
			"orlix:process_capability_probe"
		)
		XCTAssertFalse(features["process.capabilities"]?.reason.isEmpty ?? true)
		for name in [
			"process.user",
			"process.noNewPrivileges",
			"process.closeAdditionalFds",
			"process.oomScoreAdj",
			"process.scheduler",
			"process.ioPriority",
			"process.execCPUAffinity",
			"process.terminal",
			"process.consoleSize"
		] {
			XCTAssertEqual(features[name]?.status, .recognized, name)
			XCTAssertNil(features[name]?.proof, name)
			XCTAssertFalse(features[name]?.reason.isEmpty ?? true, name)
		}
		XCTAssertEqual(features["seccomp"]?.status, .deterministicallyRejected)
		XCTAssertEqual(features["seccomp"]?.proof, "orlix:runtime_config_parser")
		XCTAssertEqual(features["intelRdt"]?.status, .deterministicallyRejected)
		XCTAssertEqual(features["intelRdt"]?.proof, "orlix:runtime_config_parser")
        XCTAssertEqual(features["ociCgroupPath"]?.status, .implemented)
        XCTAssertEqual(features["ociCgroupPath"]?.proof, "orlix:init_cgroup_path_join")
        XCTAssertEqual(features["ociHugepageLimits"]?.status, .deterministicallyRejected)
        XCTAssertEqual(features["ociHugepageLimits"]?.proof, "orlix:runtime_config_parser")
        XCTAssertEqual(features["ociLinuxDevices"]?.status, .implemented)
        XCTAssertEqual(features["ociLinuxDevices"]?.proof, "orlix:device_node_probe")
		XCTAssertEqual(features["ociLinuxResources"]?.status, .recognized)
XCTAssertEqual(features["ociLinuxResources"]?.proof, "orlix:runtime_config_parser")
		XCTAssertEqual(features["ociPersonality"]?.status, .recognized)
		XCTAssertNil(features["ociPersonality"]?.proof)
        XCTAssertEqual(features["ociTimeNamespace"]?.status, .implemented)
        XCTAssertEqual(features["ociTimeNamespace"]?.proof, "orlix:time_namespace_probe")
		XCTAssertEqual(features["ociTimeOffsets"]?.status, .recognized)
		XCTAssertNil(features["ociTimeOffsets"]?.proof)
        XCTAssertEqual(features["userNamespaceMappings"]?.status, .implemented)
        XCTAssertEqual(features["userNamespaceMappings"]?.proof, "orlix:user_namespace_probe")
        XCTAssertEqual(features["ociCPUQuota"]?.status, .implemented)
XCTAssertEqual(features["ociCPUQuota"]?.proof, "orlix:cgroup_cpu_probe")
XCTAssertEqual(features["ociCPUShares"]?.status, .implemented)
XCTAssertEqual(features["ociCPUShares"]?.proof, "orlix:cgroup_cpu_probe")
XCTAssertEqual(features["ociPidsLimit"]?.status, .implemented)
XCTAssertEqual(features["ociPidsLimit"]?.proof, "orlix:cgroup_pids_probe")
		XCTAssertEqual(features["ociMemoryLimit"]?.status, .implemented)
		XCTAssertEqual(features["ociMemoryLimit"]?.proof, "orlix:cgroup_memory_probe")
XCTAssertEqual(features["ociBlockIOControls"]?.status, .implemented)
XCTAssertEqual(features["ociBlockIOControls"]?.proof, "orlix:cgroup_io_probe")
		XCTAssertEqual(features["ociNamespaces"]?.status, .recognized)
        XCTAssertEqual(features["ociNamespaces"]?.proof, "orlix:runtime_config_parser")
        XCTAssertEqual(features["ociMountIpcUtsNetworkCgroupPidNamespaces"]?.status, .implemented)
        XCTAssertEqual(
            features["ociMountIpcUtsNetworkCgroupPidNamespaces"]?.proof,
            "orlix:mount_namespace_probe,orlix:ipc_namespace_probe,orlix:network_namespace_probe,orlix:cgroup_namespace_probe,orlix:namespace_probe"
        )
XCTAssertEqual(features["ociReadonlyPaths"]?.status, .implemented)
XCTAssertEqual(features["ociReadonlyPaths"]?.proof, "orlix:rootinit_readonly_paths")
		XCTAssertEqual(features["ociUnifiedCgroupResources"]?.status, .implemented)
		XCTAssertEqual(features["ociUnifiedCgroupResources"]?.proof, "orlix:cgroup_unified_probe")
		XCTAssertEqual(features["ociLifecycleStateModel"]?.status, .recognized)
		XCTAssertNil(features["ociLifecycleStateModel"]?.proof)
		XCTAssertEqual(features["ociRuntimeSpecLifecycle"]?.status, .recognized)
		XCTAssertNil(features["ociRuntimeSpecLifecycle"]?.proof)
		XCTAssertEqual(features["root.readonly"]?.status, .implemented)
		XCTAssertEqual(features["root.readonly"]?.proof, "orlix:readonly_root_probe")
		XCTAssertEqual(features["virtioFsHostFolderMount"]?.status, .implemented)
		XCTAssertEqual(features["virtioFsHostFolderMount"]?.proof, "orlix:virtio_fs_mount_probe")
        XCTAssertEqual(features["userNamespaceMappings"]?.status, .implemented)
        XCTAssertEqual(features["userNamespaceMappings"]?.proof, "orlix:user_namespace_probe")
        XCTAssertEqual(features["idmappedMounts"]?.status, .deterministicallyRejected)
        XCTAssertEqual(features["selinux"]?.status, .deterministicallyRejected)
    }

	func testOCIRuntimeFeatureReportEncodesStableJSON() throws {
		let data = try OrlixOCIRuntimeFeatureReport.current.jsonData()
		let json = String(decoding: data, as: UTF8.self)

		XCTAssertTrue(json.contains(#""platform" : "linux/arm64""#))
		XCTAssertTrue(json.contains(#""name" : "fdAliases""#))
		XCTAssertTrue(json.contains(#""proof" : "orlix:fd_alias_probe""#))
		XCTAssertTrue(json.contains(#""name" : "procfs""#))
		XCTAssertTrue(json.contains(#""proof" : "orlix:pseudo_fs_probe""#))
		XCTAssertTrue(json.contains(#""name" : "root.readonly""#))
		XCTAssertTrue(json.contains(#""proof" : "orlix:readonly_root_probe""#))
		XCTAssertTrue(json.contains(#""name" : "rootfsPropagation""#))
		XCTAssertTrue(json.contains(#""proof" : "orlix:rootinit_mount_propagation""#))
		XCTAssertTrue(json.contains(#""name" : "netDevices""#))
		XCTAssertTrue(json.contains(#""name" : "ociLifecycleStateModel""#))
		XCTAssertTrue(json.contains(#""name" : "ociRuntimeSpecLifecycle""#))
		XCTAssertTrue(json.contains(#""name" : "process.terminal""#))
		XCTAssertTrue(json.contains(#""name" : "virtioNetDevicePlane""#))
		XCTAssertTrue(json.contains(#""name" : "virtioFsHostFolderMount""#))
		XCTAssertTrue(json.contains(#""proof" : "orlix:virtio_fs_mount_probe""#))
		XCTAssertTrue(json.contains(#""status" : "deterministicallyRejected""#))

		let decoded = try JSONDecoder().decode(
			OrlixOCIRuntimeFeatureReport.self,
			from: data
		)
		XCTAssertEqual(decoded, OrlixOCIRuntimeFeatureReport.current)
		XCTAssertEqual(
			decoded.features.map(\.name),
			decoded.features.map(\.name).sorted()
		)
	}

	func testOCIRuntimeConfigParserConvertsMinimalLinuxConfig() throws {
		let config = Data(
#"{"ociVersion":"1.1.0","annotations":{"org.opencontainers.image.ref.name":"orlix-demo"},"root":{"path":"rootfs","readonly":false},"mounts":[{"destination":"/proc","type":"proc","source":"proc"},{"destination":"/sys/fs/cgroup","type":"cgroup2","source":"cgroup2"},{"destination":"/tmp","type":"tmpfs","source":"tmpfs"}],"linux":{"rootfsPropagation":"rshared","namespaces":[{"type":"mount"},{"type":"ipc"},{"type":"uts"},{"type":"network","path":"/proc/1/ns/net"},{"type":"cgroup"}],"maskedPaths":["/proc/kcore","/sys/firmware"],"readonlyPaths":["/proc/sys","/sys"],"devices":[{"path":"/dev/orlix-null","type":"c","major":1,"minor":3,"fileMode":438,"uid":0,"gid":0},{"path":"/dev/orlix-pipe","type":"p","fileMode":420,"uid":0,"gid":0}],"cgroupsPath":"/orlix/demo","resources":{"pids":{"limit":64},"cpu":{"shares":1024,"quota":50000,"period":100000},"memory":{"limit":268435456},"blockIO":{"weight":100,"weightDevice":[{"major":1,"minor":0,"weight":200}],"throttleReadBpsDevice":[{"major":1,"minor":0,"rate":1048576}],"throttleWriteIOPSDevice":[{"major":1,"minor":0,"rate":120}]},"unified":{"cpu.weight":"39","memory.max":"268435456"}},"sysctl":{"kernel.hostname":"orlix-demo","net.ipv4.ip_forward":"1"}},"process":{"terminal":true,"noNewPrivileges":true,"closeAdditionalFds":true,"oomScoreAdj":-500,"scheduler":{"policy":"SCHED_FIFO","priority":1},"ioPriority":{"class":"IOPRIO_CLASS_BE","priority":4},"execCPUAffinity":{"initial":"0","final":"0-1"},"consoleSize":{"height":24,"width":80},"args":["/bin/sh","-lc","echo ok"],"env":["PATH=/usr/bin:/bin","TERM=xterm-256color"],"cwd":"/work","user":{"uid":1000,"gid":1000,"umask":18},"rlimits":[{"type":"RLIMIT_NOFILE","soft":64,"hard":64}]}}"#.utf8
		)

		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)

		XCTAssertEqual(descriptor.ociVersion, "1.1.0")
		XCTAssertEqual(
			descriptor.annotations["org.opencontainers.image.ref.name"],
			"orlix-demo"
		)
        XCTAssertEqual(descriptor.rootPath, "rootfs")
        XCTAssertFalse(descriptor.rootReadonly)
        XCTAssertEqual(descriptor.rootPropagation, .shared)
        XCTAssertEqual(descriptor.maskedPaths, ["/proc/kcore", "/sys/firmware"])
		XCTAssertEqual(descriptor.readonlyPaths, ["/proc/sys", "/sys"])
		XCTAssertEqual(descriptor.cgroupsPath, "/orlix/demo")
		XCTAssertEqual(descriptor.cgroupPidsLimit, 64)
XCTAssertEqual(
descriptor.cgroupCPUMax,
OrlixEnvironmentCgroupCPUMax(quotaMicros: 50_000, periodMicros: 100_000)
)
XCTAssertEqual(descriptor.cgroupCPUWeight, 39)
XCTAssertEqual(descriptor.cgroupMemoryMax, 268_435_456)
XCTAssertEqual(descriptor.sysctls["kernel.hostname"], "orlix-demo")
        XCTAssertEqual(descriptor.sysctls["net.ipv4.ip_forward"], "1")
		XCTAssertEqual(descriptor.mounts.count, 3)
		XCTAssertEqual(descriptor.mounts[0].destination, "/proc")
		XCTAssertEqual(descriptor.mounts[0].type, "proc")
		XCTAssertEqual(descriptor.mounts[0].source, "proc")
		XCTAssertEqual(descriptor.mounts[0].options, [])
		XCTAssertEqual(descriptor.mounts[1].destination, "/sys/fs/cgroup")
		XCTAssertEqual(descriptor.mounts[1].type, "cgroup2")
		XCTAssertEqual(descriptor.mounts[1].source, "cgroup2")
		XCTAssertEqual(descriptor.mounts[1].options, [])
		XCTAssertEqual(descriptor.mounts[2].destination, "/tmp")
		XCTAssertEqual(descriptor.mounts[2].type, "tmpfs")
		XCTAssertEqual(descriptor.defaultCommand, ["/bin/sh", "-lc", "echo ok"])
		XCTAssertEqual(descriptor.defaultEnvironment["PATH"], "/usr/bin:/bin")
		XCTAssertEqual(descriptor.defaultEnvironment["TERM"], "xterm-256color")
		XCTAssertEqual(descriptor.defaultWorkingDirectory, "/work")
		XCTAssertEqual(descriptor.defaultUserID, 1000)
		XCTAssertEqual(descriptor.defaultGroupID, 1000)
		XCTAssertTrue(descriptor.defaultNoNewPrivileges)
		XCTAssertTrue(descriptor.defaultCloseAdditionalFds)
		XCTAssertEqual(descriptor.defaultOOMScoreAdjustment, -500)
		XCTAssertEqual(
			descriptor.defaultScheduler,
			OrlixEnvironmentScheduler(policy: "SCHED_FIFO", priority: 1)
		)
		XCTAssertEqual(
			descriptor.defaultIOPriority,
			OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_BE", priority: 4)
		)
		XCTAssertEqual(
			descriptor.defaultCPUAffinity,
			OrlixEnvironmentCPUAffinity(mask: "0-1")
		)
		XCTAssertEqual(descriptor.defaultUmask, 18)
		XCTAssertEqual(descriptor.defaultRlimits, [
			OrlixEnvironmentRlimit(type: "RLIMIT_NOFILE", soft: 64, hard: 64)
		])
		XCTAssertTrue(descriptor.terminal)
		XCTAssertEqual(descriptor.consoleSize, OrlixOCIRuntimeConsoleSize(height: 24, width: 80))
		XCTAssertEqual(descriptor.namespaces, ["cgroup", "ipc", "mount", "uts"])
		XCTAssertEqual(descriptor.namespacePaths, ["network": "/proc/1/ns/net"])
		XCTAssertEqual(descriptor.cgroupCPUWeight, 39)
		XCTAssertEqual(descriptor.cgroupMemoryMax, 268_435_456)
		XCTAssertEqual(descriptor.cgroupIOWeight, 100)
        XCTAssertEqual(descriptor.cgroupUnified, [
            OrlixEnvironmentCgroupUnifiedEntry(file: "cpu.weight", value: "39"),
            OrlixEnvironmentCgroupUnifiedEntry(file: "memory.max", value: "268435456"),
            OrlixEnvironmentCgroupUnifiedEntry(file: "io.weight", value: "1:0 200"),
            OrlixEnvironmentCgroupUnifiedEntry(file: "io.max", value: "1:0 rbps=1048576"),
            OrlixEnvironmentCgroupUnifiedEntry(file: "io.max", value: "1:0 wiops=120")
        ])
        XCTAssertEqual(descriptor.deviceNodes, [
            OrlixEnvironmentDeviceNode(
                path: "/dev/orlix-null",
                type: "c",
                major: 1,
                minor: 3,
                fileMode: 0o666,
                uid: 0,
                gid: 0
            ),
            OrlixEnvironmentDeviceNode(
                path: "/dev/orlix-pipe",
                type: "p",
                fileMode: 0o644,
                uid: 0,
                gid: 0
            )
        ])

		let environment = try descriptor.environmentDescriptor(
			id: "oci-runtime-config",
			rootMount: .defaultOverlay
		)
		XCTAssertEqual(environment.id, "oci-runtime-config")
		XCTAssertEqual(environment.source, .ociLayout)
		XCTAssertEqual(environment.platform, "linux/arm64")
		XCTAssertEqual(environment.rootImageIdentifier, "oci-runtime-config")
		XCTAssertEqual(environment.defaultCommand, descriptor.defaultCommand)
		XCTAssertEqual(environment.defaultWorkingDirectory, descriptor.defaultWorkingDirectory)
		XCTAssertEqual(environment.defaultUserID, descriptor.defaultUserID)
		XCTAssertEqual(environment.defaultGroupID, descriptor.defaultGroupID)
		XCTAssertEqual(environment.defaultNoNewPrivileges, descriptor.defaultNoNewPrivileges)
		XCTAssertEqual(environment.defaultCloseAdditionalFds, descriptor.defaultCloseAdditionalFds)
		XCTAssertEqual(environment.defaultTerminalRows, descriptor.consoleSize?.height)
		XCTAssertEqual(environment.defaultTerminalColumns, descriptor.consoleSize?.width)
		XCTAssertEqual(environment.defaultOOMScoreAdjustment, descriptor.defaultOOMScoreAdjustment)
		XCTAssertEqual(environment.defaultScheduler, descriptor.defaultScheduler)
		XCTAssertEqual(environment.defaultIOPriority, descriptor.defaultIOPriority)
		XCTAssertEqual(environment.defaultCPUAffinity, descriptor.defaultCPUAffinity)
        XCTAssertEqual(environment.defaultUmask, descriptor.defaultUmask)
        XCTAssertEqual(environment.defaultRlimits, descriptor.defaultRlimits)
        XCTAssertEqual(environment.rootPropagation, .shared)
        XCTAssertEqual(environment.maskedPaths, descriptor.maskedPaths)
        XCTAssertEqual(environment.readonlyPaths, descriptor.readonlyPaths)
        XCTAssertEqual(environment.cgroupsPath, descriptor.cgroupsPath)
        XCTAssertEqual(environment.cgroupPidsLimit, descriptor.cgroupPidsLimit)
        XCTAssertEqual(environment.namespaces, descriptor.namespaces)
		XCTAssertEqual(environment.namespacePaths, descriptor.namespacePaths)
		XCTAssertEqual(environment.sysctls, descriptor.sysctls)
		XCTAssertEqual(environment.cgroupCPUMax, descriptor.cgroupCPUMax)
		XCTAssertEqual(environment.cgroupCPUWeight, descriptor.cgroupCPUWeight)
        XCTAssertEqual(environment.cgroupMemoryMax, descriptor.cgroupMemoryMax)
        XCTAssertEqual(environment.cgroupIOWeight, descriptor.cgroupIOWeight)
		XCTAssertEqual(environment.cgroupUnified, descriptor.cgroupUnified)
		XCTAssertEqual(environment.deviceNodes, descriptor.deviceNodes)

		let targetRootEnvironment = try descriptor.environmentDescriptor(
			id: "oci-runtime-config",
			rootMount: .defaultOverlay,
			rootImageIdentifier: "orlix.test.environment.oci-runtime-test-fixture"
		)
		XCTAssertEqual(
			targetRootEnvironment.rootImageIdentifier,
			"orlix.test.environment.oci-runtime-test-fixture"
		)
	}

func testOCIRuntimeConfigParserNormalizesRelativeCgroupsPath() throws {
	let config = Data(
		"""
		{
		  "ociVersion": "1.1.0",
		  "process": { "args": ["/bin/sh"], "cwd": "/" },
		  "root": { "path": "rootfs" },
		  "linux": {
		    "cgroupsPath": "demo.slice/orlix",
		    "resources": { "pids": { "limit": 12 } }
		  }
		}
		""".utf8
	)
	let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
	XCTAssertEqual(descriptor.cgroupsPath, "/orlix/demo.slice/orlix")
	XCTAssertEqual(descriptor.cgroupPidsLimit, 12)

	let environment = try descriptor.environmentDescriptor(
		id: "oci-relative-cgroup-path",
		rootMount: .defaultOverlay
	)
	XCTAssertEqual(environment.cgroupsPath, "/orlix/demo.slice/orlix")
	XCTAssertEqual(environment.cgroupPidsLimit, 12)
	let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
		descriptor: environment,
		kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
	)
	XCTAssertTrue(
		try XCTUnwrap(commandLine).contains(
			"orlix.cgroups.path=/orlix/demo.slice/orlix"
		)
	)
	XCTAssertTrue(
		try XCTUnwrap(commandLine).contains("orlix.cgroups.pids.max=12")
	)
}

func testOCIRuntimeConfigParserDerivesDefaultCgroupsPathForResources() throws {
	let config = Data(
		"""
		{
		  "ociVersion": "1.1.0",
		  "process": { "args": ["/bin/sh"], "cwd": "/" },
		  "root": { "path": "rootfs" },
		  "linux": {
		    "resources": {
		      "pids": { "limit": 64 },
		      "memory": { "limit": 268435456 },
		      "cpu": { "quota": 50000, "period": 100000, "shares": 1024 },
		      "blockIO": {
		        "weight": 100,
		        "weightDevice": [{ "major": 8, "minor": 0, "weight": 200 }],
		        "throttleReadBpsDevice": [
		          { "major": 8, "minor": 0, "rate": 1048576 }
		        ]
		      },
				"unified": {
					"cpu.pressure": "some 100000 100000",
					"cpu.weight": "39"
				}
		    }
		  }
		}
		""".utf8
	)

	let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
	XCTAssertNil(descriptor.cgroupsPath)
	XCTAssertEqual(descriptor.cgroupPidsLimit, 64)
	XCTAssertEqual(descriptor.cgroupMemoryMax, 268_435_456)
	XCTAssertEqual(
		descriptor.cgroupCPUMax,
		OrlixEnvironmentCgroupCPUMax(quotaMicros: 50_000, periodMicros: 100_000)
	)
	XCTAssertEqual(descriptor.cgroupCPUWeight, 39)
	XCTAssertEqual(descriptor.cgroupIOWeight, 100)
	XCTAssertEqual(
		descriptor.cgroupUnified,
		[
			OrlixEnvironmentCgroupUnifiedEntry(file: "cpu.pressure", value: "some 100000 100000"),
			OrlixEnvironmentCgroupUnifiedEntry(file: "cpu.weight", value: "39"),
			OrlixEnvironmentCgroupUnifiedEntry(file: "io.weight", value: "8:0 200"),
			OrlixEnvironmentCgroupUnifiedEntry(file: "io.max", value: "8:0 rbps=1048576"),
		]
	)

	let environment = try descriptor.environmentDescriptor(
		id: "oci-default-cgroup-path",
		rootMount: .defaultOverlay
	)
	XCTAssertEqual(environment.cgroupsPath, "/orlix/oci/oci-default-cgroup-path")
	XCTAssertEqual(environment.cgroupPidsLimit, descriptor.cgroupPidsLimit)
	XCTAssertEqual(environment.cgroupMemoryMax, descriptor.cgroupMemoryMax)
	XCTAssertEqual(environment.cgroupCPUMax, descriptor.cgroupCPUMax)
	XCTAssertEqual(environment.cgroupCPUWeight, descriptor.cgroupCPUWeight)
	XCTAssertEqual(environment.cgroupIOWeight, descriptor.cgroupIOWeight)
	XCTAssertEqual(environment.cgroupUnified, descriptor.cgroupUnified)

	let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
		descriptor: environment,
		kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
	)
	let unwrappedCommandLine = try XCTUnwrap(commandLine)
	XCTAssertTrue(
		unwrappedCommandLine.contains(
			"orlix.cgroups.path=/orlix/oci/oci-default-cgroup-path"
		)
	)
	XCTAssertTrue(unwrappedCommandLine.contains("orlix.cgroups.pids.max=64"))
	XCTAssertTrue(unwrappedCommandLine.contains("orlix.cgroups.memory.max=268435456"))
	XCTAssertTrue(unwrappedCommandLine.contains("orlix.cgroups.cpu.max=50000%20100000"))
	XCTAssertTrue(unwrappedCommandLine.contains("orlix.cgroups.cpu.weight=39"))
	XCTAssertTrue(unwrappedCommandLine.contains("orlix.cgroups.io.weight=100"))
	XCTAssertTrue(
		unwrappedCommandLine.contains("orlix.cgroups.unified0=cpu.pressure=some%20100000%20100000")
	)
	XCTAssertTrue(
		unwrappedCommandLine.contains("orlix.cgroups.unified1=cpu.weight=39")
	)
	XCTAssertTrue(
		unwrappedCommandLine.contains("orlix.cgroups.unified2=io.weight=8:0%20200")
	)
	XCTAssertTrue(
		unwrappedCommandLine.contains("orlix.cgroups.unified3=io.max=8:0%20rbps=1048576")
	)
}

func testOCIRuntimeConfigParserCarriesLinuxPersonality() throws {
	let config = Data(
            """
            {
              "ociVersion": "1.1.0",
              "process": { "args": ["/bin/sh"], "cwd": "/" },
              "root": { "path": "rootfs" },
              "linux": {
                "personality": { "domain": "LINUX32" }
              }
            }
            """.utf8
        )

        let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
        XCTAssertEqual(descriptor.defaultPersonalityDomain, "LINUX32")

        let environment = try descriptor.environmentDescriptor(
            id: "oci-personality",
            rootMount: .defaultOverlay
        )
        XCTAssertEqual(environment.defaultPersonalityDomain, "LINUX32")

        let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
            descriptor: environment,
            kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
        )
        XCTAssertTrue(
            try XCTUnwrap(commandLine).contains("orlix.personality=LINUX32")
        )
    }

    func testOCIRuntimeConfigParserRejectsUnsupportedLinuxPersonalityShapes() throws {
        let fragments: [(feature: String, json: String)] = [
            (
                "personality.domain",
                #""personality": { "domain": "BSD" }"#
            ),
            (
                "personality.domain",
                #""personality": {}"#
            ),
            (
                "personality.flags",
                #""personality": { "domain": "LINUX", "flags": ["ADDR_NO_RANDOMIZE"] }"#
            )
        ]

        for (feature, linuxFragment) in fragments {
            let config = Data(
                """
                {
                  "ociVersion": "1.1.0",
                  "process": { "args": ["/bin/sh"], "cwd": "/" },
                  "root": { "path": "rootfs" },
                  "linux": { \(linuxFragment) }
                }
                """.utf8
            )

            XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
                XCTAssertEqual(
                    error as? OrlixOCIRuntimeConfigError,
                    .unsupportedLinuxFeature("linux.\(feature)")
                )
            }
        }
    }

func testOCIRuntimeConfigParserCarriesTimeNamespaceOffsets() throws {
    let config = Data(
        """
            {
              "ociVersion": "1.1.0",
              "process": { "args": ["/bin/sh"], "cwd": "/" },
              "root": { "path": "rootfs" },
              "linux": {
                "namespaces": [{ "type": "time" }],
                "timeOffsets": {
                  "boottime": { "secs": -3, "nanosecs": 250 },
                  "monotonic": { "secs": 12, "nanosecs": 500000000 }
                }
              }
            }
            """.utf8
        )

        let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
        XCTAssertEqual(descriptor.namespaces, ["time"])
        XCTAssertEqual(
            descriptor.timeOffsets,
            [
                OrlixEnvironmentTimeOffset(clock: "boottime", secs: -3, nanosecs: 250),
                OrlixEnvironmentTimeOffset(clock: "monotonic", secs: 12, nanosecs: 500000000)
            ]
        )

        let environment = try descriptor.environmentDescriptor(
            id: "oci-time-offsets",
            rootMount: .defaultOverlay
        )
        let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
            descriptor: environment,
            kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
        )
        XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.namespace0=time"))
        XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.timeoffset0=boottime:-3:250"))
    XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.timeoffset1=monotonic:12:500000000"))
}

func testOCIRuntimeConfigParserCarriesPIDNamespace() throws {
    let config = Data(
        """
        {
          "ociVersion": "1.1.0",
          "process": { "args": ["/bin/sh"], "cwd": "/" },
          "root": { "path": "rootfs" },
          "linux": {
            "namespaces": [{ "type": "pid" }]
          }
        }
        """.utf8
    )

    let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
    XCTAssertEqual(descriptor.namespaces, ["pid"])

    let environment = try descriptor.environmentDescriptor(
        id: "oci-pid-namespace",
        rootMount: .defaultOverlay
    )
    let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
        descriptor: environment,
        kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
    )
    XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.namespace0=pid"))
}

func testOCIRuntimeConfigParserCarriesUserNamespaceMappings() throws {
    let config = Data(
        """
            {
                "ociVersion": "1.1.0",
                "process": { "args": ["/bin/sh"], "cwd": "/" },
                "root": { "path": "rootfs" },
                "linux": {
                    "namespaces": [{ "type": "user" }],
                    "uidMappings": [{ "containerID": 0, "hostID": 501, "size": 1 }],
                    "gidMappings": [{ "containerID": 0, "hostID": 20, "size": 1 }]
                }
            }
            """.utf8
        )

        let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
        XCTAssertEqual(descriptor.namespaces, ["user"])
        XCTAssertEqual(
            descriptor.uidMappings,
            [OrlixEnvironmentIDMapping(containerID: 0, hostID: 501, size: 1)]
        )
        XCTAssertEqual(
            descriptor.gidMappings,
            [OrlixEnvironmentIDMapping(containerID: 0, hostID: 20, size: 1)]
        )

        let environment = try descriptor.environmentDescriptor(
            id: "oci-user-map",
            rootMount: .defaultOverlay
        )
        XCTAssertEqual(environment.namespaces, descriptor.namespaces)
        XCTAssertEqual(environment.uidMappings, descriptor.uidMappings)
        XCTAssertEqual(environment.gidMappings, descriptor.gidMappings)

        let commandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
            descriptor: environment,
            kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine
        )
        XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.namespace0=user"))
        XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.uidmap0=0:501:1"))
        XCTAssertTrue(try XCTUnwrap(commandLine).contains("orlix.gidmap0=0:20:1"))
    }

    func testOCIRuntimeConfigParserRejectsInvalidTimeOffsets() throws {
        let fragments: [(feature: String, linux: String)] = [
            (
                "timeOffsets.namespace",
                #""timeOffsets": { "monotonic": { "secs": 1, "nanosecs": 0 } }"#
            ),
            (
                "timeOffsets.realtime",
                #""namespaces": [{ "type": "time" }], "timeOffsets": { "realtime": { "secs": 1, "nanosecs": 0 } }"#
            ),
            (
                "timeOffsets.monotonic.nanosecs",
                #""namespaces": [{ "type": "time" }], "timeOffsets": { "monotonic": { "secs": 1, "nanosecs": 1000000000 } }"#
            )
        ]

        for (feature, linux) in fragments {
            let config = Data(
                """
                {
                  "ociVersion": "1.1.0",
                  "process": { "args": ["/bin/sh"], "cwd": "/" },
                  "root": { "path": "rootfs" },
                  "linux": { \(linux) }
                }
                """.utf8
            )

            XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
                XCTAssertEqual(
                    error as? OrlixOCIRuntimeConfigError,
                    .unsupportedLinuxFeature("linux.\(feature)")
                )
            }
        }
    }

    func testOCIRuntimeConfigParserRejectsInvalidRootPaths() throws {
	let invalidConfigs: [(Data, OrlixOCIRuntimeConfigError)] = [
		(
			Data(#"{ "ociVersion": "1.1.0", "process": { "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
			.missingRootPath
		),
		(
			Data(#"{ "ociVersion": "1.1.0", "root": { "path": "" }, "process": { "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
			.invalidRootPath("")
		),
			(
				Data(#"{ "ociVersion": "1.1.0", "root": { "path": "root\u0000fs" }, "process": { "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
				.invalidRootPath("root\u{0}fs")
			)
		]

		for (config, expectedError) in invalidConfigs {
			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
				XCTAssertEqual(error as? OrlixOCIRuntimeConfigError, expectedError)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedLinuxFeatures() throws {
		let unsupportedFeatureConfigs: [(String, String)] = [
            ("uidMappings.namespace", #""uidMappings": [{ "containerID": 0, "hostID": 0, "size": 1 }]"#),
            ("gidMappings.namespace", #""gidMappings": [{ "containerID": 0, "hostID": 0, "size": 1 }]"#),
            ("devices.path", #""devices": [{ "type": "c", "major": 1, "minor": 3 }]"#),
            ("devices.path", #""devices": [{ "path": "dev/null", "type": "c", "major": 1, "minor": 3 }]"#),
            ("devices.type", #""devices": [{ "path": "/dev/orlix-null", "type": "x", "major": 1, "minor": 3 }]"#),
            ("devices.major", #""devices": [{ "path": "/dev/orlix-null", "type": "c", "minor": 3 }]"#),
            ("devices.fileMode", #""devices": [{ "path": "/dev/orlix-null", "type": "c", "major": 1, "minor": 3, "fileMode": 32768 }]"#),
("resources.memory.limit", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "limit": -2 } }"#),
("resources.memory.reservation", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "reservation": 134217728 } }"#),
("resources.memory.swap", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "swap": 536870912 } }"#),
("resources.memory.kernel", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "kernel": 67108864 } }"#),
("resources.memory.kernelTCP", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "kernelTCP": 67108864 } }"#),
("resources.memory.swappiness", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "swappiness": 60 } }"#),
("resources.memory.disableOOMKiller", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "disableOOMKiller": true } }"#),
("resources.memory.useHierarchy", #""cgroupsPath": "/orlix/demo", "resources": { "memory": { "useHierarchy": true } }"#),
("resources.cpu.quota", #""cgroupsPath": "/orlix/demo", "resources": { "cpu": { "quota": 0, "period": 100000 } }"#),
			("resources.cpu.period", #""cgroupsPath": "/orlix/demo", "resources": { "cpu": { "quota": 50000, "period": 0 } }"#),
			("resources.cpu.shares", #""cgroupsPath": "/orlix/demo", "resources": { "cpu": { "shares": 1 } }"#),
			("resources.cpu.cpus", #""cgroupsPath": "/orlix/demo", "resources": { "cpu": { "cpus": "0" } }"#),
		("resources.blockIO.weight", #""cgroupsPath": "/orlix/demo", "resources": { "blockIO": { "weight": 0 } }"#),
		("resources.blockIO.leafWeight", #""cgroupsPath": "/orlix/demo", "resources": { "blockIO": { "leafWeight": 100 } }"#),
		("resources.blockIO.weightDevice.weight", #""cgroupsPath": "/orlix/demo", "resources": { "blockIO": { "weightDevice": [{ "major": 1, "minor": 0, "weight": 0 }] } }"#),
		("resources.blockIO.weightDevice.leafWeight", #""cgroupsPath": "/orlix/demo", "resources": { "blockIO": { "weightDevice": [{ "major": 1, "minor": 0, "leafWeight": 100 }] } }"#),
		("resources.blockIO.throttleReadBpsDevice.rate", #""cgroupsPath": "/orlix/demo", "resources": { "blockIO": { "throttleReadBpsDevice": [{ "major": 1, "minor": 0, "rate": 1 }] } }"#),
            ("resources.pids.limit", #""cgroupsPath": "/orlix/demo", "resources": { "pids": { "limit": -2 } }"#),
            ("seccomp", #""seccomp": { "defaultAction": "SCMP_ACT_ERRNO" }"#),
            ("mountLabel", #""mountLabel": "system_u:object_r:container_file_t:s0""#),
            ("namespaces.user.path", #""namespaces": [{ "type": "user", "path": "/proc/1/ns/user" }]"#),
            ("namespaces.mount.duplicate", #""namespaces": [{ "type": "mount" }, { "type": "mount" }]"#),
            ("netDevices", #""netDevices": [{ "name": "eth0" }]"#)
        ]

		for (feature, linuxFragment) in unsupportedFeatureConfigs {
			let config = Data(
				"""
            {
                "ociVersion": "1.1.0",
                "process": { "args": ["/bin/sh"], "cwd": "/" },
                "root": { "path": "rootfs" },
                "linux": { \(linuxFragment) }
            }
            """.utf8
			)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("linux.\(feature)")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedProcessFeatures() throws {
		let unsupportedProcessConfigs: [(String, String)] = [
			("capabilities", #""capabilities": { "bounding": ["CAP_ORLIX_ONLY"] }"#),
			("apparmorProfile", #""apparmorProfile": "container-default""#),
			("hooks.prestart", #""hooks": { "prestart": [{ "path": "/usr/bin/prepare-container" }] }"#),
			("hooks.createRuntime", #""hooks": { "createRuntime": [{ "path": "/usr/bin/create-runtime" }] }"#),
			("hooks.createContainer", #""hooks": { "createContainer": [{ "path": "/usr/bin/create-container" }] }"#),
			("hooks.startContainer", #""hooks": { "startContainer": [{ "path": "/usr/bin/start-container" }] }"#),
			("hooks.poststart", #""hooks": { "poststart": [{ "path": "/usr/bin/poststart" }] }"#),
			("hooks.poststop", #""hooks": { "poststop": [{ "path": "/usr/bin/poststop" }] }"#),
			("selinuxLabel", #""selinuxLabel": "system_u:system_r:container_t:s0""#)
		]

		for (feature, processFragment) in unsupportedProcessConfigs {
			let config: Data
			let expectedFeature: String
			if feature.hasPrefix("hooks.") {
				config = Data(
					"""
					{
					  "ociVersion": "1.1.0",
					  "process": {
					    "args": ["/bin/sh"],
					    "cwd": "/"
					  },
					  \(processFragment)
					}
					""".utf8
				)
				expectedFeature = feature
			} else {
				config = Data(
					"""
					{
					  "ociVersion": "1.1.0",
					  "process": {
					    "args": ["/bin/sh"],
					    "cwd": "/",
					    \(processFragment)
					  }
					}
					""".utf8
				)
				expectedFeature = feature == "capabilities"
					? "process.capabilities.bounding"
					: "process.\(feature)"
			}
			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature(expectedFeature)
				)
			}
		}
	}

	func testOCIRuntimeConfigParserAcceptsRootReadonly() throws {
		let config = Data("""
		{
		  "ociVersion": "1.1.0",
		  "process": {
		    "args": ["/bin/sh"],
		    "cwd": "/",
		    "env": ["PATH=/usr/bin"]
		  },
		  "root": { "path": "rootfs", "readonly": true }
		}
		""".utf8)

		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)

		XCTAssertTrue(descriptor.rootReadonly)
	}

	func testOCIRuntimeConfigParserAcceptsHostnameAndDomainname() throws {
		let config = Data("""
		{
			"ociVersion": "1.1.0",
		  "hostname": "container-host",
		  "domainname": "example.test",
		  "process": {
		    "args": ["/bin/sh"],
		    "cwd": "/",
		    "env": ["PATH=/usr/bin"]
		  },
		  "root": { "path": "rootfs" }
		}
		""".utf8)

		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
		XCTAssertEqual(descriptor.hostname, "container-host")
		XCTAssertEqual(descriptor.domainname, "example.test")
	}

	func testOCIRuntimeConfigParserRejectsDuplicateEnvironmentKeys() throws {
		let config = Data("""
		{
			"ociVersion": "1.1.0",
			"process": {
				"args": ["/bin/sh"],
				"cwd": "/",
				"env": ["PATH=/usr/bin", "PATH=/bin"]
			},
			"root": { "path": "rootfs" }
		}
		""".utf8)

		XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeConfigError,
				.invalidEnvironmentEntry("PATH=/bin")
			)
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedNonLinuxPlatformConfig() throws {
		let fragments: [(feature: String, json: String)] = [
			("solaris", #""solaris": {}"#),
			("windows", #""windows": {}"#),
			("vm", #""vm": {}"#),
			("zOS", #""zOS": {}"#)
		]

		for (feature, configFragment) in fragments {
			let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  \(configFragment),
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/"
			  },
			  "root": { "path": "rootfs" }
			}
			""".utf8)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature(feature)
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedProcessRlimits() throws {
		let fields: [(String, String)] = [
			("unknown", #""rlimits": [{ "type": "RLIMIT_UNKNOWN", "hard": 64, "soft": 64 }]"#),
			("reversed", #""rlimits": [{ "type": "RLIMIT_NOFILE", "hard": 32, "soft": 64 }]"#),
			("duplicate", #""rlimits": [{ "type": "RLIMIT_NOFILE", "hard": 64, "soft": 64 }, { "type": "RLIMIT_NOFILE", "hard": 64, "soft": 64 }]"#)
		]

		for (feature, field) in fields {
			let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/",
			    \(field)
			  },
			  "root": { "path": "rootfs" }
			}
			""".utf8)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("process.rlimits")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedProcessSchedulingFields() throws {
		let fragments: [(feature: String, json: String)] = [
			("oomScoreAdj", #""oomScoreAdj": 1001"#),
			("scheduler.nice", #""scheduler": { "policy": "SCHED_OTHER", "nice": 4 }"#),
			("scheduler.flags", #""scheduler": { "policy": "SCHED_OTHER", "flags": ["RESET_ON_FORK"] }"#),
			("ioPriority.class", #""ioPriority": { "class": "IOPRIO_CLASS_NONE", "priority": 4 }"#),
			("ioPriority.priority", #""ioPriority": { "class": "IOPRIO_CLASS_BE", "priority": 8 }"#),
			("execCPUAffinity", #""execCPUAffinity": { "initial": "0", "final": "1-0" }"#)
		]

		for (feature, processFragment) in fragments {
			let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/",
			    \(processFragment)
			  },
			  "root": { "path": "rootfs" }
			}
			""".utf8)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("process.\(feature)")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsOutOfRangeProcessUserUmask() throws {
		let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
		    "args": ["/bin/sh"],
		    "cwd": "/",
		    "user": {
		      "uid": 0,
		      "gid": 0,
		      "umask": 512
		    }
		  },
		  "root": { "path": "rootfs" }
		}
		""".utf8)

		XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeConfigError,
				.unsupportedLinuxFeature("process.user.umask")
			)
		}
	}

	func testOCIRuntimeConfigParserTranslatesPIDAndUserNamespacePathJoins() throws {
		let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/",
			    "env": ["PATH=/usr/bin:/bin"]
			  },
			  "root": { "path": "rootfs" },
			  "linux": {
			    "namespaces": [
			      { "type": "pid", "path": "/proc/1/ns/pid" },
			      { "type": "user", "path": "/proc/1/ns/user" }
			    ]
			  }
			}
			""".utf8)

		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)

		XCTAssertEqual(descriptor.namespaces, [])
		XCTAssertEqual(
			descriptor.namespacePaths,
			[
				"pid": "/proc/1/ns/pid",
				"user": "/proc/1/ns/user",
			]
		)

		let environment = try descriptor.environmentDescriptor(
			id: "oci-namespace-paths",
			rootMount: .defaultOverlay
		)
		XCTAssertEqual(environment.namespaces, [])
		XCTAssertEqual(environment.namespacePaths, descriptor.namespacePaths)
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedHooks() throws {
		let hookFields = [
			"prestart",
			"createRuntime",
			"createContainer",
			"startContainer",
			"poststart",
			"poststop"
		]

		for hookField in hookFields {
			let config = Data("""
			{
				"ociVersion": "1.1.0",
				"process": {
					"args": ["/bin/sh"],
					"cwd": "/",
					"env": ["PATH=/usr/bin"]
				},
				"root": { "path": "rootfs" },
				"hooks": {
					"\(hookField)": [
						{ "path": "/usr/bin/prepare-container", "args": ["prepare-container"] }
					]
				}
			}
			""".utf8)

			XCTAssertThrowsError(
				try OrlixOCIRuntimeConfigParser().parse(config),
				hookField
			) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("hooks.\(hookField)")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedMountIDMappings() throws {
		let fragments: [(feature: String, json: String)] = [
			("uidMappings", #""uidMappings": [{ "containerID": 0, "hostID": 1000, "size": 1 }]"#),
			("gidMappings", #""gidMappings": [{ "containerID": 0, "hostID": 1000, "size": 1 }]"#)
		]

		for (feature, mountFragment) in fragments {
			let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/"
			  },
			  "root": { "path": "rootfs" },
			  "mounts": [
			    {
			      "destination": "/proc",
			      "type": "proc",
			      \(mountFragment)
			    }
			  ]
			}
			""".utf8)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("mounts.\(feature)")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedLinuxRuntimeFields() throws {
		let fragments: [(feature: String, json: String)] = [
            ("unified", #""unified": { "memory.max": "1048576" }"#),
			("intelRdt", #""intelRdt": { "l3CacheSchema": "L3:0=ff" }"#),
			("hugepageLimits", #""hugepageLimits": [{ "pageSize": "2MB", "limit": 1 }]"#),
			("rdma", #""rdma": { "mlx5_0": { "hcaHandles": 1, "hcaObjects": 1 } }"#)
		]

		for (feature, linuxFragment) in fragments {
			let config = Data("""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/"
			  },
			  "root": { "path": "rootfs" },
			  "linux": {
			    \(linuxFragment)
			  }
			}
			""".utf8)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config), feature) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeConfigError,
					.unsupportedLinuxFeature("linux.\(feature)")
				)
			}
		}
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedVersionAndConsoleSize() throws {
		let invalidConfigs: [(Data, OrlixOCIRuntimeConfigError)] = [
			(
				Data(#"{ "ociVersion": "0.2.0", "process": { "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
				.unsupportedOCIVersion("0.2.0")
			),
			(
				Data(#"{ "ociVersion": "1.1.0", "process": { "terminal": false, "consoleSize": { "height": 24, "width": 80 }, "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
				.invalidConsoleSize
			),
			(
				Data(#"{ "ociVersion": "1.1.0", "process": { "terminal": true, "consoleSize": { "height": 0, "width": 80 }, "args": ["/bin/sh"], "cwd": "/" } }"#.utf8),
				.invalidConsoleSize
			)
		]

		for (config, expectedError) in invalidConfigs {
			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
				XCTAssertEqual(error as? OrlixOCIRuntimeConfigError, expectedError)
			}
		}
	}

	func testOCIRuntimeBundleLoadsConfigAndRootfs() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try fileManager.createDirectory(
			at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
			withIntermediateDirectories: true
		)
		try minimalOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)

		XCTAssertEqual(bundle.bundleURL, bundleURL)
		XCTAssertEqual(bundle.configURL.lastPathComponent, "config.json")
		XCTAssertEqual(bundle.rootfsURL.lastPathComponent, "rootfs")
		XCTAssertEqual(bundle.config.ociVersion, "1.1.0")
		XCTAssertEqual(
			try bundle.config.environmentDescriptor(
				id: "bundle-test",
				rootMount: .defaultOverlay,
				mounts: []
			).defaultCommand,
			["/bin/sh"]
		)

		let controller = bundle.lifecycleController(id: "bundle-test")
		XCTAssertEqual(controller.record.id, "bundle-test")
		XCTAssertEqual(controller.record.bundlePath, bundleURL.path)
		XCTAssertEqual(controller.record.state, .configured)

		let session = try bundle.sessionDescriptor(
			id: "bundle-test",
			rootMount: .defaultOverlay
		)
		XCTAssertEqual(session.id, "bundle-test")
	XCTAssertEqual(session.lifecycleState, .created)
	XCTAssertEqual(session.environment.defaultCommand, ["/bin/sh"])
}

func testOCIRegistryImageReferenceParsesDistributionEndpoints() throws {
	let digest = "sha256:\(String(repeating: "a", count: 64))"
	let tagged = try OrlixOCIRegistryImageReference(
		"registry.example.org:5000/library/alpine:3.20"
	)
	XCTAssertEqual(tagged.scheme, "https")
	XCTAssertEqual(tagged.registry, "registry.example.org:5000")
	XCTAssertEqual(tagged.repository, "library/alpine")
	XCTAssertEqual(tagged.tag, "3.20")
	XCTAssertNil(tagged.digest)
	XCTAssertEqual(tagged.manifestReference, "3.20")
	XCTAssertEqual(
		try tagged.manifestURL().absoluteString,
		"https://registry.example.org:5000/v2/library/alpine/manifests/3.20"
	)
	XCTAssertEqual(
		try tagged.blobURL(digest: digest).absoluteString,
		"https://registry.example.org:5000/v2/library/alpine/blobs/\(digest)"
	)

	let digested = try OrlixOCIRegistryImageReference(
		"https://ghcr.io/rudironsoni/orlix@\(digest)"
	)
	XCTAssertEqual(digested.scheme, "https")
	XCTAssertEqual(digested.registry, "ghcr.io")
	XCTAssertEqual(digested.repository, "rudironsoni/orlix")
	XCTAssertNil(digested.tag)
	XCTAssertEqual(digested.digest, digest)
	XCTAssertEqual(digested.manifestReference, digest)
	XCTAssertEqual(
		try digested.manifestURL().absoluteString,
		"https://ghcr.io/v2/rudironsoni/orlix/manifests/\(digest)"
	)

	let dockerScheme = try OrlixOCIRegistryImageReference(
		"docker://docker.io/library/alpine:3.20"
	)
	XCTAssertEqual(dockerScheme.scheme, "https")
	XCTAssertEqual(dockerScheme.registry, "docker.io")
	XCTAssertEqual(dockerScheme.repository, "library/alpine")
	XCTAssertEqual(dockerScheme.tag, "3.20")
	XCTAssertEqual(
		try dockerScheme.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/3.20"
	)

	let dockerDefaultScheme = try OrlixOCIRegistryImageReference(
		"alpine:3.20",
		defaultScheme: "docker"
	)
	XCTAssertEqual(dockerDefaultScheme.scheme, "https")
	XCTAssertEqual(dockerDefaultScheme.registry, "docker.io")
	XCTAssertEqual(dockerDefaultScheme.repository, "library/alpine")
	XCTAssertEqual(
		try dockerDefaultScheme.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/3.20"
	)

	let implicitLatest = try OrlixOCIRegistryImageReference("localhost:5000/orlix/rootfs")
	XCTAssertNil(implicitLatest.tag)
	XCTAssertEqual(implicitLatest.manifestReference, "latest")
	XCTAssertEqual(
		try implicitLatest.manifestURL().absoluteString,
		"https://localhost:5000/v2/orlix/rootfs/manifests/latest"
	)

	let dockerOfficial = try OrlixOCIRegistryImageReference("alpine:3.20")
	XCTAssertEqual(dockerOfficial.scheme, "https")
	XCTAssertEqual(dockerOfficial.registry, "docker.io")
	XCTAssertEqual(dockerOfficial.repository, "library/alpine")
	XCTAssertEqual(dockerOfficial.tag, "3.20")
	XCTAssertEqual(dockerOfficial.manifestReference, "3.20")
	XCTAssertEqual(
		try dockerOfficial.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/3.20"
	)

	let dockerNamespace = try OrlixOCIRegistryImageReference(
		"rudironsoni/orlix:latest"
	)
	XCTAssertEqual(dockerNamespace.registry, "docker.io")
	XCTAssertEqual(dockerNamespace.repository, "rudironsoni/orlix")
	XCTAssertEqual(dockerNamespace.tag, "latest")
	XCTAssertEqual(
		try dockerNamespace.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/rudironsoni/orlix/manifests/latest"
	)

	let dockerEndpoint = try OrlixOCIRegistryImageReference(
		"registry-1.docker.io/library/alpine:latest"
	)
	XCTAssertEqual(dockerEndpoint.registry, "docker.io")
	XCTAssertEqual(dockerEndpoint.repository, "library/alpine")
	XCTAssertEqual(dockerEndpoint.tag, "latest")
	XCTAssertEqual(
		try dockerEndpoint.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/latest"
	)

	let explicitDockerOfficial = try OrlixOCIRegistryImageReference(
		"docker.io/alpine:latest"
	)
	XCTAssertEqual(explicitDockerOfficial.registry, "docker.io")
	XCTAssertEqual(explicitDockerOfficial.repository, "library/alpine")
	XCTAssertEqual(explicitDockerOfficial.tag, "latest")
	XCTAssertEqual(
		try explicitDockerOfficial.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/latest"
	)

	let legacyDockerEndpoint = try OrlixOCIRegistryImageReference(
		"index.docker.io/alpine:latest"
	)
	XCTAssertEqual(legacyDockerEndpoint.registry, "docker.io")
	XCTAssertEqual(legacyDockerEndpoint.repository, "library/alpine")
	XCTAssertEqual(
		try legacyDockerEndpoint.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/latest"
	)
}

func testOCIRegistryImageReferenceRejectsInvalidInput() throws {
	let digest = "sha256:\(String(repeating: "b", count: 64))"
	let invalidReferences: [(String, OrlixOCIRegistryReferenceError)] = [
		("", .emptyReference),
		("registry.example.org/Upper/Name:latest", .invalidRepository("Upper/Name")),
		("registry.example.org/library/alpine:bad tag", .invalidTag("bad tag")),
		("registry.example.org/library/alpine@sha256:bad", .invalidDigest("sha256:bad")),
		("ftp://registry.example.org/library/alpine:latest", .unsupportedScheme("ftp")),
		(
			"https://registry.example.org/library/alpine:latest?x=1",
			.invalidEndpoint("https://registry.example.org/library/alpine:latest?x=1")
		),
	]
	for (reference, expectedError) in invalidReferences {
		XCTAssertThrowsError(try OrlixOCIRegistryImageReference(reference)) { error in
			XCTAssertEqual(error as? OrlixOCIRegistryReferenceError, expectedError)
		}
	}
	let valid = try OrlixOCIRegistryImageReference("registry.example.org/library/alpine")
	let invalidDigest = String(digest.dropLast()) + "x"
	XCTAssertThrowsError(try valid.blobURL(digest: invalidDigest)) { error in
		XCTAssertEqual(
			error as? OrlixOCIRegistryReferenceError,
			.invalidDigest(invalidDigest)
		)
	}
}

	func testOCIEnvironmentRunArgumentsParsesOrlixRunCommand() throws {
		let arguments = try OrlixOCIEnvironmentRunArguments([
			"orlix",
			"run",
		"--id",
		"demo-alpine",
		"--platform",
		"linux/arm64/v8",
		"alpine:3.20",
		"--",
		"/bin/sh",
		"-lc",
		"echo hello",
	])

    XCTAssertEqual(arguments.image, "alpine:3.20")
	XCTAssertEqual(arguments.id, "demo-alpine")
	XCTAssertEqual(arguments.platform, "linux/arm64/v8")
	XCTAssertNil(arguments.entrypoint)
	XCTAssertTrue(arguments.environment.isEmpty)
	XCTAssertNil(arguments.workingDirectory)
	XCTAssertNil(arguments.userID)
	XCTAssertNil(arguments.groupID)
	XCTAssertTrue(arguments.supplementaryGroupIDs.isEmpty)
	XCTAssertNil(arguments.capabilities)
	XCTAssertNil(arguments.hostname)
	XCTAssertNil(arguments.domainname)
	XCTAssertNil(arguments.terminal)
	XCTAssertNil(arguments.terminalRows)
	XCTAssertNil(arguments.terminalColumns)
	XCTAssertNil(arguments.rootReadonly)
	XCTAssertNil(arguments.rootPropagation)
	XCTAssertTrue(arguments.rlimits.isEmpty)
	XCTAssertTrue(arguments.sysctls.isEmpty)
	XCTAssertTrue(arguments.maskedPaths.isEmpty)
	XCTAssertTrue(arguments.readonlyPaths.isEmpty)
	XCTAssertNil(arguments.umask)
	XCTAssertNil(arguments.oomScoreAdjustment)
	XCTAssertNil(arguments.scheduler)
	XCTAssertNil(arguments.ioPriority)
	XCTAssertNil(arguments.cpuAffinity)
	XCTAssertNil(arguments.personalityDomain)
	XCTAssertNil(arguments.noNewPrivileges)
	XCTAssertNil(arguments.closeAdditionalFds)
	XCTAssertNil(arguments.cgroupsPath)
	XCTAssertNil(arguments.cgroupPidsLimit)
	XCTAssertNil(arguments.cgroupCPUMax)
	XCTAssertNil(arguments.cgroupCPUWeight)
	XCTAssertNil(arguments.cgroupMemoryMax)
	XCTAssertNil(arguments.cgroupIOWeight)
	XCTAssertTrue(arguments.cgroupUnified.isEmpty)
	XCTAssertTrue(arguments.tmpfsMounts.isEmpty)
	XCTAssertTrue(arguments.mounts.isEmpty)
	XCTAssertTrue(arguments.deviceNodes.isEmpty)
	XCTAssertTrue(arguments.namespaces.isEmpty)
	XCTAssertTrue(arguments.namespacePaths.isEmpty)
	XCTAssertTrue(arguments.timeOffsets.isEmpty)
	XCTAssertTrue(arguments.uidMappings.isEmpty)
	XCTAssertTrue(arguments.gidMappings.isEmpty)
	XCTAssertEqual(arguments.command, ["/bin/sh", "-lc", "echo hello"])
}

func testOCIEnvironmentRunArgumentsAcceptsCommonOptionSpellings() throws {
		let nameArguments = try OrlixOCIEnvironmentRunArguments([
			"orlix",
			"run",
			"--name",
			"named-alpine",
			"--rm",
			"--platform=linux/arm64/v8",
			"alpine:3.20",
			"/bin/echo",
			"hello",
		])

    XCTAssertEqual(nameArguments.image, "alpine:3.20")
	XCTAssertEqual(nameArguments.id, "named-alpine")
	XCTAssertEqual(nameArguments.platform, "linux/arm64/v8")
	XCTAssertNil(nameArguments.entrypoint)
	XCTAssertTrue(nameArguments.environment.isEmpty)
	XCTAssertNil(nameArguments.workingDirectory)
	XCTAssertNil(nameArguments.userID)
	XCTAssertNil(nameArguments.groupID)
	XCTAssertTrue(nameArguments.supplementaryGroupIDs.isEmpty)
	XCTAssertNil(nameArguments.capabilities)
	XCTAssertNil(nameArguments.hostname)
	XCTAssertNil(nameArguments.domainname)
	XCTAssertNil(nameArguments.terminal)
	XCTAssertTrue(nameArguments.rlimits.isEmpty)
	XCTAssertNil(nameArguments.umask)
	XCTAssertNil(nameArguments.oomScoreAdjustment)
	XCTAssertNil(nameArguments.scheduler)
	XCTAssertNil(nameArguments.ioPriority)
	XCTAssertNil(nameArguments.cpuAffinity)
	XCTAssertNil(nameArguments.personalityDomain)
	XCTAssertNil(nameArguments.noNewPrivileges)
	XCTAssertNil(nameArguments.closeAdditionalFds)
	XCTAssertNil(nameArguments.cgroupsPath)
	XCTAssertNil(nameArguments.cgroupPidsLimit)
	XCTAssertNil(nameArguments.cgroupCPUMax)
	XCTAssertNil(nameArguments.cgroupCPUWeight)
	XCTAssertNil(nameArguments.cgroupMemoryMax)
	XCTAssertNil(nameArguments.cgroupIOWeight)
	XCTAssertEqual(nameArguments.command, ["/bin/echo", "hello"])
    XCTAssertTrue(nameArguments.removeAfterRun)

		let equalsArguments = try OrlixOCIEnvironmentRunArguments([
			"run",
			"--id=equals-alpine",
			"alpine:3.20",
		])

    XCTAssertEqual(equalsArguments.image, "alpine:3.20")
	XCTAssertEqual(equalsArguments.id, "equals-alpine")
	XCTAssertEqual(equalsArguments.platform, "linux/arm64")
	XCTAssertNil(equalsArguments.entrypoint)
	XCTAssertTrue(equalsArguments.environment.isEmpty)
	XCTAssertNil(equalsArguments.workingDirectory)
	XCTAssertNil(equalsArguments.userID)
	XCTAssertNil(equalsArguments.groupID)
	XCTAssertTrue(equalsArguments.supplementaryGroupIDs.isEmpty)
	XCTAssertNil(equalsArguments.capabilities)
	XCTAssertNil(equalsArguments.hostname)
	XCTAssertNil(equalsArguments.domainname)
	XCTAssertNil(equalsArguments.terminal)
	XCTAssertTrue(equalsArguments.rlimits.isEmpty)
	XCTAssertNil(equalsArguments.umask)
	XCTAssertNil(equalsArguments.oomScoreAdjustment)
	XCTAssertNil(equalsArguments.scheduler)
	XCTAssertNil(equalsArguments.ioPriority)
	XCTAssertNil(equalsArguments.cpuAffinity)
	XCTAssertNil(equalsArguments.personalityDomain)
	XCTAssertNil(equalsArguments.noNewPrivileges)
	XCTAssertNil(equalsArguments.closeAdditionalFds)
	XCTAssertNil(equalsArguments.cgroupsPath)
	XCTAssertNil(equalsArguments.cgroupPidsLimit)
	XCTAssertNil(equalsArguments.cgroupCPUMax)
	XCTAssertNil(equalsArguments.cgroupCPUWeight)
	XCTAssertNil(equalsArguments.cgroupMemoryMax)
	XCTAssertNil(equalsArguments.cgroupIOWeight)
	XCTAssertNil(equalsArguments.command)
    XCTAssertFalse(equalsArguments.removeAfterRun)
}

func testOCIEnvironmentRunArgumentsAcceptsEntrypointOverride() throws {
    let splitArguments = try OrlixOCIEnvironmentRunArguments([
        "orlix",
        "run",
        "--entrypoint",
        "/usr/bin/env",
        "alpine:3.20",
        "sh",
        "-lc",
        "echo entrypoint",
    ])

    XCTAssertEqual(splitArguments.image, "alpine:3.20")
    XCTAssertEqual(splitArguments.entrypoint, ["/usr/bin/env"])
    XCTAssertEqual(splitArguments.command, ["sh", "-lc", "echo entrypoint"])

    let equalsArguments = try OrlixOCIEnvironmentRunArguments([
        "run",
        "--entrypoint=/bin/busybox",
        "alpine:3.20",
    ])

    XCTAssertEqual(equalsArguments.image, "alpine:3.20")
	XCTAssertEqual(equalsArguments.entrypoint, ["/bin/busybox"])
	XCTAssertNil(equalsArguments.command)
}

func testOCIEnvironmentRunArgumentsAcceptsEnvironmentAndWorkdirOverrides()
	throws
{
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--env",
		"TERM=xterm-256color",
		"-e",
		"EMPTY=",
		"--env=PATH=/usr/local/bin:/usr/bin:/bin",
		"--workdir",
		"/workspace",
		"alpine:3.20",
		"/bin/pwd",
	])

	XCTAssertEqual(arguments.image, "alpine:3.20")
	XCTAssertEqual(
		arguments.environment,
		[
			"TERM": "xterm-256color",
			"EMPTY": "",
			"PATH": "/usr/local/bin:/usr/bin:/bin",
		]
	)
	XCTAssertEqual(arguments.workingDirectory, "/workspace")
	XCTAssertEqual(arguments.command, ["/bin/pwd"])

	let shortWorkdirArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"-w",
		"/tmp",
		"alpine:3.20",
	])

	XCTAssertEqual(shortWorkdirArguments.workingDirectory, "/tmp")
}

func testOCIEnvironmentRunArgumentsAcceptsNumericUserOverrides() throws {
	let userArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--user",
		"1000:100",
		"alpine:3.20",
	])

	XCTAssertEqual(userArguments.userID, 1000)
	XCTAssertEqual(userArguments.groupID, 100)

	let shortUserArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"-u",
		"42",
		"alpine:3.20",
	])

	XCTAssertEqual(shortUserArguments.userID, 42)
	XCTAssertNil(shortUserArguments.groupID)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--user=7:8",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.userID, 7)
	XCTAssertEqual(equalsArguments.groupID, 8)
}

func testOCIEnvironmentRunArgumentsAcceptsSupplementaryGroupOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--group-add",
		"44",
		"--group-add=45",
		"--group-add",
		"44",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.supplementaryGroupIDs, [44, 45])
}

func testOCIEnvironmentRunArgumentsRejectsInvalidSupplementaryGroupOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--group-add",
			"wheel",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.user.additionalGids")
		)
	}
}

func testOCIEnvironmentListArgumentsAcceptsCommandAndStateFilters() throws {
	let list = try OrlixOCIEnvironmentListArguments([
		"orlix",
		"list",
	])
	XCTAssertEqual(list.states, [])

	let ps = try OrlixOCIEnvironmentListArguments([
		"ps",
		"--state",
		"running",
		"--status=stopped",
		"--state=running",
	])
	XCTAssertEqual(ps.states, [.running, .stopped])
}

func testOCIEnvironmentListArgumentsRejectsInvalidInput() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentListArguments(["state"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentListArgumentsError,
			.missingListCommand
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentListArguments(["ps", "--state"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentListArgumentsError,
			.missingOptionValue("--state")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentListArguments(["ps", "--state=paused"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentListArgumentsError,
			.invalidState("paused")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentListArguments(["ps", "oci-demo"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentListArgumentsError,
			.unexpectedArgument("oci-demo")
		)
	}
}

func testOCIEnvironmentStateArgumentsAcceptsIDForms() throws {
    let positional = try OrlixOCIEnvironmentStateArguments([
        "orlix",
        "state",
        "oci-demo",
	])
	XCTAssertEqual(positional.id, "oci-demo")

	let idOption = try OrlixOCIEnvironmentStateArguments([
		"state",
		"--id",
		"oci-by-id",
	])
	XCTAssertEqual(idOption.id, "oci-by-id")

	let nameOption = try OrlixOCIEnvironmentStateArguments([
		"state",
		"--name=oci-by-name",
    ])
    XCTAssertEqual(nameOption.id, "oci-by-name")
}

func testOCIEnvironmentInstallerInspectReturnsDescriptorAndLifecycleState()
    throws
{
    let fileManager = FileManager.default
    let scratch = fileManager.temporaryDirectory.appendingPathComponent(
        "orlix-oci-inspect-\(UUID().uuidString)",
        isDirectory: true
    )
    try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
    defer { try? fileManager.removeItem(at: scratch) }

    let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
    let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
    try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
    try nonRootOCIRuntimeConfig().write(
        to: bundleURL.appendingPathComponent("config.json")
    )

    let registry = OrlixEnvironmentRegistry(
        linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
        cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
        scratchRoot: scratch.appendingPathComponent(
            "runtime-scratch",
            isDirectory: true
        )
    )
    _ = try OrlixOCIRuntime(registry: registry).create(
        bundleURL: bundleURL,
        id: "oci-inspect"
    )

    let inspected = try OrlixOCIEnvironmentInstaller(registry: registry).inspect(
        arguments: ["orlix", "inspect", "--id", "oci-inspect"],
        fileManager: fileManager
    )

    XCTAssertEqual(inspected.id, "oci-inspect")
    XCTAssertEqual(inspected.platform, "linux/arm64")
    XCTAssertEqual(inspected.defaultCommand, ["/usr/bin/id"])
    XCTAssertEqual(inspected.defaultWorkingDirectory, "/work")
    XCTAssertEqual(inspected.defaultUserID, 1000)
    XCTAssertEqual(inspected.defaultGroupID, 1000)
    XCTAssertEqual(inspected.lifecycleState, .created)
    XCTAssertEqual(inspected.stateReport.id, "oci-inspect")
    XCTAssertEqual(inspected.stateReport.status, .created)
}

func testOCIEnvironmentExecArgumentsAcceptsIDAndCommandForms() throws {
    let positional = try OrlixOCIEnvironmentExecArguments([
        "orlix",
        "exec",
        "oci-demo",
        "/bin/echo",
        "hello",
    ])
    XCTAssertEqual(positional.id, "oci-demo")
    XCTAssertEqual(positional.command, ["/bin/echo", "hello"])

    let idOption = try OrlixOCIEnvironmentExecArguments([
        "exec",
        "--id",
        "oci-by-id",
        "--",
        "/bin/sh",
        "-lc",
        "echo ok",
    ])
    XCTAssertEqual(idOption.id, "oci-by-id")
    XCTAssertEqual(idOption.command, ["/bin/sh", "-lc", "echo ok"])

    let nameOption = try OrlixOCIEnvironmentExecArguments([
        "exec",
        "--name=oci-by-name",
        "/usr/bin/env",
        "true",
    ])
    XCTAssertEqual(nameOption.id, "oci-by-name")
    XCTAssertEqual(nameOption.command, ["/usr/bin/env", "true"])
}

func testOCIEnvironmentExecArgumentsRejectsInvalidInput() throws {
    XCTAssertThrowsError(
        try OrlixOCIEnvironmentExecArguments(["run", "oci-demo", "/bin/true"])
    ) { error in
        XCTAssertEqual(
            error as? OrlixOCIEnvironmentExecArgumentsError,
            .missingExecCommand
        )
    }
    XCTAssertThrowsError(try OrlixOCIEnvironmentExecArguments(["exec"])) {
        error in
        XCTAssertEqual(
            error as? OrlixOCIEnvironmentExecArgumentsError,
            .missingID
        )
    }
    XCTAssertThrowsError(
        try OrlixOCIEnvironmentExecArguments(["exec", "oci-demo"])
    ) { error in
        XCTAssertEqual(
            error as? OrlixOCIEnvironmentExecArgumentsError,
            .missingCommand
        )
    }
    XCTAssertThrowsError(
        try OrlixOCIEnvironmentExecArguments(["exec", "--id"])
    ) { error in
        XCTAssertEqual(
            error as? OrlixOCIEnvironmentExecArgumentsError,
            .missingOptionValue("--id")
        )
    }
}

func testOCIEnvironmentInstallerExecRunsCommandInPreparedEnvironment()
    throws
{
    let fileManager = FileManager.default
    let scratch = fileManager.temporaryDirectory.appendingPathComponent(
        "orlix-oci-exec-\(UUID().uuidString)",
        isDirectory: true
    )
    try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
    defer { try? fileManager.removeItem(at: scratch) }

    let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
    let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
    try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
    try nonRootOCIRuntimeConfig().write(
        to: bundleURL.appendingPathComponent("config.json")
    )

    let registry = OrlixEnvironmentRegistry(
        linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
        cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
        scratchRoot: scratch.appendingPathComponent(
            "runtime-scratch",
            isDirectory: true
        )
    )
    let created = try OrlixOCIRuntime(registry: registry).create(
        bundleURL: bundleURL,
        id: "oci-exec"
    )
    try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
    try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)

    let driver = try RecordingOCIRuntimeProcessObservationDriver(
        startPID: 84,
        completion: .exited(
            OrlixOCIRuntimeProcessExitObservation(pid: 84, exitStatus: 0)
        )
    )
    let result = try OrlixOCIEnvironmentInstaller(registry: registry).exec(
        arguments: [
            "orlix",
            "exec",
            "oci-exec",
            "--",
            "/bin/echo",
            "hello",
        ],
        terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
        using: driver,
        fileManager: fileManager
    )

    XCTAssertEqual(result.id, "oci-exec")
    XCTAssertEqual(result.command, ["/bin/echo", "hello"])
    XCTAssertEqual(result.runResult.startedStateReport.status, .running)
    XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
    XCTAssertEqual(result.runResult.completedStateReport.exitStatus, 0)
    XCTAssertEqual(driver.startCommands, [["/bin/echo", "hello"]])
    XCTAssertEqual(driver.events, ["start:created:nil", "wait:running:84"])
}

func testOCIEnvironmentStateArgumentsRejectsInvalidInput() throws {
    XCTAssertThrowsError(
        try OrlixOCIEnvironmentStateArguments(["run", "oci-demo"])
    ) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentStateArgumentsError,
			.missingStateCommand
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentStateArguments(["state"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentStateArgumentsError,
			.missingID
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentStateArguments(["state", "--id"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentStateArgumentsError,
			.missingOptionValue("--id")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentStateArguments(["state", "--unknown", "oci-demo"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentStateArgumentsError,
			.unknownOption("--unknown")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentStateArguments(["state", "one", "two"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentStateArgumentsError,
			.unexpectedArgument("two")
		)
	}
}

func testOCIEnvironmentLifecycleArgumentsAcceptIDForms() throws {
	let positional = try OrlixOCIEnvironmentLifecycleArguments(
		["orlix", "start", "oci-demo"],
		command: "start"
	)
	XCTAssertEqual(positional.id, "oci-demo")

	let idOption = try OrlixOCIEnvironmentLifecycleArguments(
		["wait", "--id", "oci-wait"],
		command: "wait"
	)
	XCTAssertEqual(idOption.id, "oci-wait")

	let nameOption = try OrlixOCIEnvironmentLifecycleArguments(
		["delete", "--name=oci-delete"],
		command: "delete"
	)
	XCTAssertEqual(nameOption.id, "oci-delete")
}

func testOCIEnvironmentLifecycleArgumentsRejectInvalidInput() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentLifecycleArguments(["state", "oci-demo"], command: "start")
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentLifecycleArgumentsError,
			.missingCommand("start")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentLifecycleArguments(["start"], command: "start")
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentLifecycleArgumentsError,
			.missingID
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentLifecycleArguments(["start", "--id"], command: "start")
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentLifecycleArgumentsError,
			.missingOptionValue("--id")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentLifecycleArguments(
			["start", "--unknown", "oci-demo"],
			command: "start"
		)
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentLifecycleArgumentsError,
			.unknownOption("--unknown")
		)
	}
}

func testOCIEnvironmentKillArgumentsAcceptSignalForms() throws {
	let defaultSignal = try OrlixOCIEnvironmentKillArguments([
		"orlix", "kill", "oci-demo",
	])
	XCTAssertEqual(defaultSignal.id, "oci-demo")
	XCTAssertEqual(defaultSignal.signal, 15)
	XCTAssertFalse(defaultSignal.signalSpecified)

	let positionalSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "oci-positional", "9",
	])
	XCTAssertEqual(positionalSignal.id, "oci-positional")
	XCTAssertEqual(positionalSignal.signal, 9)
	XCTAssertTrue(positionalSignal.signalSpecified)

	let optionSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "--id", "oci-option", "--signal=2",
	])
	XCTAssertEqual(optionSignal.id, "oci-option")
	XCTAssertEqual(optionSignal.signal, 2)
	XCTAssertTrue(optionSignal.signalSpecified)

	let shortSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "--name=oci-short", "-s", "15",
	])
	XCTAssertEqual(shortSignal.id, "oci-short")
	XCTAssertEqual(shortSignal.signal, 15)
	XCTAssertTrue(shortSignal.signalSpecified)

	let namedOptionSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "--id", "oci-named", "--signal=SIGKILL",
	])
	XCTAssertEqual(namedOptionSignal.id, "oci-named")
	XCTAssertEqual(namedOptionSignal.signal, 9)
	XCTAssertTrue(namedOptionSignal.signalSpecified)

	let namedShortSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "--name=oci-term", "-s", "TERM",
	])
	XCTAssertEqual(namedShortSignal.id, "oci-term")
	XCTAssertEqual(namedShortSignal.signal, 15)
	XCTAssertTrue(namedShortSignal.signalSpecified)

	let namedPositionalSignal = try OrlixOCIEnvironmentKillArguments([
		"kill", "oci-lower", "sigusr1",
	])
	XCTAssertEqual(namedPositionalSignal.id, "oci-lower")
	XCTAssertEqual(namedPositionalSignal.signal, 10)
	XCTAssertTrue(namedPositionalSignal.signalSpecified)
}

func testOCIEnvironmentKillArgumentsRejectInvalidInput() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentKillArguments(["state", "oci-demo"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentKillArgumentsError,
			.missingKillCommand
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentKillArguments(["kill"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentKillArgumentsError,
			.missingID
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentKillArguments(["kill", "oci-demo", "0"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentKillArgumentsError,
			.invalidSignal("0")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentKillArguments([
			"kill", "oci-demo", "--signal=SIGDOESNOTEXIST",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentKillArgumentsError,
			.invalidSignal("SIGDOESNOTEXIST")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentKillArguments(["kill", "oci-demo", "9", "2"])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentKillArgumentsError,
			.unexpectedArgument("2")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsAnnotationOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--annotation",
		"org.opencontainers.image.ref.name=orlix-demo",
		"--annotation=com.example.empty=",
		"--annotation=com.example.override=old",
		"--annotation=com.example.override=new",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.annotations["org.opencontainers.image.ref.name"],
		"orlix-demo"
	)
	XCTAssertEqual(arguments.annotations["com.example.empty"], "")
	XCTAssertEqual(arguments.annotations["com.example.override"], "new")
}

func testOCIEnvironmentRunArgumentsRejectsInvalidAnnotationOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--annotation",
			"=value",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.invalidAnnotationEntry("=value")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--annotation",
			"missing-separator",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.invalidAnnotationEntry("missing-separator")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCgroupPidsOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--cgroups-path",
		"demo",
		"--pids-limit",
		"64",
		"alpine:3.20",
	])
	XCTAssertEqual(arguments.cgroupsPath, "/orlix/demo")
	XCTAssertEqual(arguments.cgroupPidsLimit, 64)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--cgroups-path=/orlix/explicit",
		"--pids-limit=-1",
		"alpine:3.20",
	])
	XCTAssertEqual(equalsArguments.cgroupsPath, "/orlix/explicit")
	XCTAssertEqual(equalsArguments.cgroupPidsLimit, -1)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCgroupPidsLimit() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--pids-limit",
			"-2",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.pids.limit")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCgroupCPUOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--cpu-max",
		"50000:100000",
		"--cpu-weight",
		"39",
		"alpine:3.20",
	])
	XCTAssertEqual(
		arguments.cgroupCPUMax,
		OrlixEnvironmentCgroupCPUMax(
			quotaMicros: 50_000,
			periodMicros: 100_000
		)
	)
	XCTAssertEqual(arguments.cgroupCPUWeight, 39)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--cpu-max=max",
		"--cpu-weight=10000",
		"alpine:3.20",
	])
	XCTAssertEqual(
		equalsArguments.cgroupCPUMax,
		OrlixEnvironmentCgroupCPUMax(quotaMicros: -1)
	)
	XCTAssertEqual(equalsArguments.cgroupCPUWeight, 10_000)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCgroupCPUOverrides() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cpu-max",
			"0:100000",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.cpu.quota")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cpu-max",
			"50000:0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.cpu.period")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cpu-weight",
			"0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.cpu.shares")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCgroupMemoryOverride() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--memory-max",
		"268435456",
		"alpine:3.20",
	])
	XCTAssertEqual(arguments.cgroupMemoryMax, 268_435_456)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--memory-max=-1",
		"alpine:3.20",
	])
	XCTAssertEqual(equalsArguments.cgroupMemoryMax, -1)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCgroupMemoryOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--memory-max",
			"-2",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.memory.limit")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCgroupIOOverride() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--io-weight",
		"100",
		"alpine:3.20",
	])
	XCTAssertEqual(arguments.cgroupIOWeight, 100)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--io-weight=10000",
		"alpine:3.20",
	])
	XCTAssertEqual(equalsArguments.cgroupIOWeight, 10_000)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCgroupIOOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--io-weight",
			"0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.blockIO.weight")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCgroupUnifiedOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--cgroup-unified",
		"cpu.weight=39",
		"--cgroup-unified=memory.max=268435456",
		"--cgroup-unified=kernel.memory=1",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.cgroupUnified,
		[
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "cpu.weight",
				value: "39"
			),
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "memory.max",
				value: "268435456"
			),
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "kernel.memory",
				value: "1"
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCgroupUnifiedOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cgroup-unified",
			"cpu.weight",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.unified")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cgroup-unified",
			"cpu.weight=39",
			"--cgroup-unified=cpu.weight=40",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.resources.unified.cpu.weight")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsTmpfsMountOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--tmpfs",
		"/run/cache:nosuid,nodev,noexec,size=64m,mode=0755,uid=0,gid=0",
		"--tmpfs=/var/tmp:ro",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.tmpfsMounts,
		[
			try OrlixEnvironmentTmpfsMount(
				targetPath: "/run/cache",
				readOnly: false,
				noSuid: true,
				noDev: true,
				noExec: true,
				data: "size=64m,mode=0755,uid=0,gid=0"
			),
			try OrlixEnvironmentTmpfsMount(
				targetPath: "/var/tmp",
				readOnly: true
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidTmpfsMountOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--tmpfs",
			"relative:nosuid",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.destination")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--tmpfs",
			"/run/cache:exec",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.options")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--tmpfs",
			"/run/cache:nosuid",
			"--tmpfs=/run/cache:nodev",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.destination")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsBindMountOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--mount",
		"type=bind,source=orlix:documents,target=/mnt/documents,readonly,noexec",
		"--mount=type=bind,src=/private/tmp/orlix-host,dst=/mnt/host,ro",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.mounts,
		[
			try OrlixEnvironmentMount.documents(
				targetPath: "/mnt/documents",
				readOnly: true,
				noExec: true
			),
			try OrlixEnvironmentMount.hostPath(
				"/private/tmp/orlix-host",
				targetPath: "/mnt/host",
				readOnly: true
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsAcceptsVolumeMountOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--volume",
		"documents:/mnt/documents:ro,noexec",
		"-v",
		"/private/tmp/orlix-host:/mnt/host:readonly",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.mounts,
		[
			try OrlixEnvironmentMount.documents(
				targetPath: "/mnt/documents",
				readOnly: true,
				noExec: true
			),
			try OrlixEnvironmentMount.hostPath(
				"/private/tmp/orlix-host",
				targetPath: "/mnt/host",
				readOnly: true
			),
		]
	)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--volume=/private/tmp/orlix-cache:/mnt/cache:rw",
		"alpine:3.20",
	])
	XCTAssertEqual(
		equalsArguments.mounts,
		[
			try OrlixEnvironmentMount.hostPath(
				"/private/tmp/orlix-cache",
				targetPath: "/mnt/cache"
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidVolumeMountOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--volume",
			"documents",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"-v",
			"documents:/proc/host:ro",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.destination")
		)
	}
}

func testOCIEnvironmentRunArgumentsRejectsInvalidBindMountOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--mount",
			"type=tmpfs,target=/mnt/cache",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.type.tmpfs")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--mount",
			"type=bind,source=orlix:documents,target=/proc/host",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("mounts.destination")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsDeviceNodeOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--device-node",
		"/dev/orlix-zero:c:1:5:0660:1000:100",
		"--device-node=/dev/orlix-pipe:p:0644",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.deviceNodes,
		[
			OrlixEnvironmentDeviceNode(
				path: "/dev/orlix-zero",
				type: "c",
				major: 1,
				minor: 5,
				fileMode: 0o660,
				uid: 1000,
				gid: 100
			),
			OrlixEnvironmentDeviceNode(
				path: "/dev/orlix-pipe",
				type: "p",
				fileMode: 0o644,
				uid: 0,
				gid: 0
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidDeviceNodeOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--device-node",
			"dev/null:c:1:3",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.devices.path")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--device-node",
			"/dev/null:x:1:3",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.devices.type")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--device-node=/dev/null:c:one:3",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.devices.major")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsNamespaceOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--namespace",
		"pid",
		"--namespace=uts",
		"--namespace-path",
		"network=/proc/1/ns/net",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.namespaces, ["pid", "uts"])
	XCTAssertEqual(arguments.namespacePaths, ["network": "/proc/1/ns/net"])
}

func testOCIEnvironmentRunArgumentsRejectsInvalidNamespaceOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--namespace",
			"invalid",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.namespaces.type")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--namespace-path",
			"network=proc/1/ns/net",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.namespaces.path")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--namespace",
			"network",
			"--namespace-path=network=/proc/1/ns/net",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.namespaces")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsTimeOffsetAndIDMappingOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--namespace",
		"time",
		"--time-offset",
		"monotonic:12:34",
		"--time-offset=boottime:-5:900000000",
		"--namespace",
		"user",
		"--uid-map",
		"0:501:1",
		"--gid-map=0:20:1",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.namespaces, ["time", "user"])
	XCTAssertEqual(
		arguments.timeOffsets,
		[
			OrlixEnvironmentTimeOffset(
				clock: "monotonic",
				secs: 12,
				nanosecs: 34
			),
			OrlixEnvironmentTimeOffset(
				clock: "boottime",
				secs: -5,
				nanosecs: 900_000_000
			),
		]
	)
	XCTAssertEqual(
		arguments.uidMappings,
		[OrlixEnvironmentIDMapping(containerID: 0, hostID: 501, size: 1)]
	)
	XCTAssertEqual(
		arguments.gidMappings,
		[OrlixEnvironmentIDMapping(containerID: 0, hostID: 20, size: 1)]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidTimeOffsetAndIDMappingOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--time-offset",
			"realtime:1:0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.timeOffsets")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--time-offset",
			"monotonic:1:1000000000",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.timeOffsets")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--uid-map",
			"0:501:0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.uidMappings")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--gid-map",
			"0:group:1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.gidMappings")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsSysctlOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--sysctl",
		"net.ipv4.ip_forward=1",
		"--sysctl=kernel.hostname=orlix-run",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.sysctls,
		[
			"kernel.hostname": "orlix-run",
			"net.ipv4.ip_forward": "1",
		]
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidSysctlOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--sysctl",
			"net/ipv4/ip_forward=1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.sysctl")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--sysctl",
			"net.ipv4.ip_forward",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.sysctl")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--sysctl",
			"net.ipv4.ip_forward=1",
			"--sysctl=net.ipv4.ip_forward=0",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.sysctl")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsMaskedAndReadonlyPathOverrides()
	throws
{
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--mask",
		"/proc/kcore",
		"--mask=/sys/firmware",
		"--readonly",
		"/proc/sys",
		"--readonly=/sys",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.maskedPaths, ["/proc/kcore", "/sys/firmware"])
	XCTAssertEqual(arguments.readonlyPaths, ["/proc/sys", "/sys"])
}

func testOCIEnvironmentRunArgumentsAcceptsRootReadonlyOverride() throws {
	let readOnlyArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--read-only",
		"alpine:3.20",
	])
	XCTAssertEqual(readOnlyArguments.rootReadonly, true)

	let readWriteArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--read-write",
		"alpine:3.20",
	])
	XCTAssertEqual(readWriteArguments.rootReadonly, false)
}

func testOCIEnvironmentRunArgumentsAcceptsRootPropagationOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--root-propagation",
		"shared",
		"alpine:3.20",
	])
	XCTAssertEqual(splitArguments.rootPropagation, .shared)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--root-propagation=slave",
		"alpine:3.20",
	])
	XCTAssertEqual(equalsArguments.rootPropagation, .slave)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidRootPropagationOverride()
throws
{
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--root-propagation",
			"recursive-shared",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.rootfsPropagation")
		)
	}
}

func testOCIEnvironmentRunArgumentsRejectsInvalidMaskedAndReadonlyPathOverride()
	throws
{
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--mask",
			"relative",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.maskedPaths")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--readonly",
			"/proc/../sys",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.readonlyPaths")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--mask",
			"/proc/kcore",
			"--mask=/proc/kcore",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.maskedPaths")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsHostnameOverride() throws {
	let hostnameArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--hostname",
		"oci-host",
		"alpine:3.20",
	])

	XCTAssertEqual(hostnameArguments.hostname, "oci-host")

	let shortHostnameArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"-h",
		"short-host",
		"alpine:3.20",
	])

	XCTAssertEqual(shortHostnameArguments.hostname, "short-host")

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--hostname=equals-host",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.hostname, "equals-host")
}

func testOCIEnvironmentRunArgumentsAcceptsDomainnameOverride() throws {
	let domainnameArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--domainname",
		"example.test",
		"alpine:3.20",
	])

	XCTAssertEqual(domainnameArguments.domainname, "example.test")

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--domainname=equals.example",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.domainname, "equals.example")
}

func testOCIEnvironmentRunArgumentsAcceptsTerminalOverrides() throws {
	let ttyArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--tty",
		"alpine:3.20",
	])

	XCTAssertEqual(ttyArguments.terminal, true)

	let shortTTYArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"-t",
		"alpine:3.20",
	])

	XCTAssertEqual(shortTTYArguments.terminal, true)

	let noTTYArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--no-tty",
		"alpine:3.20",
	])

	XCTAssertEqual(noTTYArguments.terminal, false)

	let noTerminalArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--no-terminal",
		"alpine:3.20",
	])

	XCTAssertEqual(noTerminalArguments.terminal, false)

	let sizeArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--terminal-size",
		"33x120",
		"alpine:3.20",
	])

	XCTAssertEqual(sizeArguments.terminalRows, 33)
	XCTAssertEqual(sizeArguments.terminalColumns, 120)

	let equalsSizeArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--terminal-size=24x80",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsSizeArguments.terminalRows, 24)
	XCTAssertEqual(equalsSizeArguments.terminalColumns, 80)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidTerminalSizeOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--terminal-size",
			"0x80",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.consoleSize")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--terminal-size=24:80",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.consoleSize")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsNoNewPrivilegesOverride() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--no-new-privileges",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.noNewPrivileges, true)
}

func testOCIEnvironmentRunArgumentsAcceptsCloseFdsOverride() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--close-fds",
		"alpine:3.20",
	])

	XCTAssertEqual(arguments.closeAdditionalFds, true)
}

func testOCIEnvironmentRunArgumentsAcceptsRlimitOverrides() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--ulimit",
		"nofile=64:128",
		"--ulimit",
		"RLIMIT_STACK=8388608",
		"alpine:3.20",
	])

	XCTAssertEqual(
		splitArguments.rlimits,
		[
			OrlixEnvironmentRlimit(
				type: "RLIMIT_NOFILE",
				soft: 64,
				hard: 128
			),
			OrlixEnvironmentRlimit(
				type: "RLIMIT_STACK",
				soft: 8_388_608,
				hard: 8_388_608
			),
		]
	)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--ulimit=nproc=4:16",
		"alpine:3.20",
	])

	XCTAssertEqual(
		equalsArguments.rlimits,
		[
			OrlixEnvironmentRlimit(
				type: "RLIMIT_NPROC",
				soft: 4,
				hard: 16
			),
		]
	)
}

func testOCIEnvironmentRunArgumentsAcceptsUmaskOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--umask",
		"022",
		"alpine:3.20",
	])

	XCTAssertEqual(splitArguments.umask, 0o022)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--umask=18",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.umask, 18)
}

func testOCIEnvironmentRunArgumentsAcceptsOOMScoreAdjustmentOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--oom-score-adj",
		"-500",
		"alpine:3.20",
	])

	XCTAssertEqual(splitArguments.oomScoreAdjustment, -500)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--oom-score-adj=1000",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.oomScoreAdjustment, 1000)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidOOMScoreAdjustment() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--oom-score-adj",
			"1001",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.oomScoreAdj")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsSchedulerOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--scheduler",
		"SCHED_FIFO:1",
		"alpine:3.20",
	])

	XCTAssertEqual(
		splitArguments.scheduler,
		OrlixEnvironmentScheduler(policy: "SCHED_FIFO", priority: 1)
	)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--scheduler=SCHED_OTHER",
		"alpine:3.20",
	])

	XCTAssertEqual(
		equalsArguments.scheduler,
		OrlixEnvironmentScheduler(policy: "SCHED_OTHER", priority: 0)
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidSchedulerOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--scheduler",
			"SCHED_UNKNOWN:1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.scheduler.policy")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--scheduler",
			"SCHED_FIFO:-1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.scheduler.priority")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsIOPriorityOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--io-priority",
		"IOPRIO_CLASS_BE:4",
		"alpine:3.20",
	])

	XCTAssertEqual(
		splitArguments.ioPriority,
		OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_BE", priority: 4)
	)

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--io-priority=IOPRIO_CLASS_IDLE",
		"alpine:3.20",
	])

	XCTAssertEqual(
		equalsArguments.ioPriority,
		OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_IDLE", priority: 0)
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidIOPriorityOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--io-priority",
			"IOPRIO_CLASS_UNKNOWN:1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.ioPriority.class")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--io-priority",
			"IOPRIO_CLASS_BE:8",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.ioPriority.priority")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsPersonalityOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--personality",
		"LINUX32",
		"alpine:3.20",
	])

	XCTAssertEqual(splitArguments.personalityDomain, "LINUX32")

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--personality=LINUX",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.personalityDomain, "LINUX")
}

func testOCIEnvironmentRunArgumentsRejectsInvalidPersonalityOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--personality",
			"BSD",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("linux.personality.domain")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCapabilitySetOverrides() throws {
	let arguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--cap-set",
		"bounding=CAP_CHOWN,CAP_SETUID",
		"--cap-set=permitted=CAP_CHOWN,CAP_CHOWN",
		"--cap-set",
		"effective=CAP_CHOWN",
		"--cap-set",
		"ambient=CAP_SETUID",
		"alpine:3.20",
	])

	XCTAssertEqual(
		arguments.capabilities,
		OrlixEnvironmentCapabilities(
			bounding: ["CAP_CHOWN", "CAP_SETUID"],
			permitted: ["CAP_CHOWN"],
			effective: ["CAP_CHOWN"],
			ambient: ["CAP_SETUID"]
		)
	)
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCapabilitySetOverride() throws {
	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cap-set",
			"permitted=CAP_ORLIX_ONLY",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.capabilities.permitted")
		)
	}

	XCTAssertThrowsError(
		try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cap-set",
			"unknown=CAP_CHOWN",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.capabilities.unknown")
		)
	}
}

func testOCIEnvironmentRunArgumentsAcceptsCPUAffinityOverride() throws {
	let splitArguments = try OrlixOCIEnvironmentRunArguments([
		"orlix",
		"run",
		"--cpu-affinity",
		"0,2-3",
		"alpine:3.20",
	])

	XCTAssertEqual(splitArguments.cpuAffinity, OrlixEnvironmentCPUAffinity(mask: "0,2-3"))

	let equalsArguments = try OrlixOCIEnvironmentRunArguments([
		"run",
		"--cpu-affinity=1",
		"alpine:3.20",
	])

	XCTAssertEqual(equalsArguments.cpuAffinity, OrlixEnvironmentCPUAffinity(mask: "1"))
}

func testOCIEnvironmentRunArgumentsRejectsInvalidCPUAffinityOverride() throws {
XCTAssertThrowsError(
try OrlixOCIEnvironmentRunArguments([
			"run",
			"--cpu-affinity",
			"3-1",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeConfigError,
			.unsupportedLinuxFeature("process.execCPUAffinity")
)
}
}

func testOCIEnvironmentRunArgumentsAcceptsPublishedPorts() throws {
let arguments = try OrlixOCIEnvironmentRunArguments([
"orlix",
"run",
"--publish",
"80",
"--publish=8080:80",
"-p",
"127.0.0.1:5353:53/udp",
"-p",
"8443:443/sctp",
"alpine:3.20",
])

XCTAssertEqual(arguments.publishedPorts, [
OrlixEnvironmentPublishedPort(containerPort: 80, proto: "tcp"),
OrlixEnvironmentPublishedPort(
containerPort: 80,
proto: "tcp",
hostPort: 8080
),
OrlixEnvironmentPublishedPort(
containerPort: 53,
proto: "udp",
hostPort: 5353,
hostAddress: "127.0.0.1"
),
OrlixEnvironmentPublishedPort(
containerPort: 443,
proto: "sctp",
hostPort: 8443
),
])
}

func testOCIEnvironmentRunArgumentsRejectsInvalidPublishedPorts() throws {
let invalidPorts: [(String, OrlixOCIRuntimeConfigError)] = [
("0", .unsupportedLinuxFeature("linux.ports.container")),
("70000", .unsupportedLinuxFeature("linux.ports.container")),
("8080:0", .unsupportedLinuxFeature("linux.ports.host")),
("8080:80/icmp", .unsupportedLinuxFeature("linux.ports.protocol")),
("127.0.0.1::80", .unsupportedLinuxFeature("linux.ports.host")),
("127.0.0.1/24:8080:80", .unsupportedLinuxFeature("linux.ports.hostIP")),
]

for (port, expectedError) in invalidPorts {
XCTAssertThrowsError(
try OrlixOCIEnvironmentRunArguments([
"orlix",
"run",
"--publish",
port,
"alpine:3.20",
])
) { error in
XCTAssertEqual(error as? OrlixOCIRuntimeConfigError, expectedError)
}
}

XCTAssertThrowsError(
try OrlixOCIEnvironmentRunArguments([
"orlix",
"run",
"--publish",
])
) { error in
XCTAssertEqual(
error as? OrlixOCIEnvironmentRunArgumentsError,
.missingOptionValue("--publish")
)
}
}

func testOCIEnvironmentRunArgumentsRejectsEmptyEqualsOptions() throws {
let invalidOptions: [(String, OrlixOCIEnvironmentRunArgumentsError)] = [
		("--id=", .missingOptionValue("--id")),
		("--name=", .missingOptionValue("--name")),
		("--platform=", .missingOptionValue("--platform")),
		("--entrypoint=", .missingOptionValue("--entrypoint")),
		("--env=", .missingOptionValue("--env")),
		("--workdir=", .missingOptionValue("--workdir")),
		("--user=", .missingOptionValue("--user")),
		("--group-add=", .missingOptionValue("--group-add")),
		("--cgroups-path=", .missingOptionValue("--cgroups-path")),
		("--pids-limit=", .missingOptionValue("--pids-limit")),
		("--cpu-max=", .missingOptionValue("--cpu-max")),
		("--cpu-weight=", .missingOptionValue("--cpu-weight")),
		("--memory-max=", .missingOptionValue("--memory-max")),
("--io-weight=", .missingOptionValue("--io-weight")),
("--cgroup-unified=", .missingOptionValue("--cgroup-unified")),
("--tmpfs=", .missingOptionValue("--tmpfs")),
("--publish=", .missingOptionValue("--publish")),
("--sysctl=", .missingOptionValue("--sysctl")),
		("--mask=", .missingOptionValue("--mask")),
		("--readonly=", .missingOptionValue("--readonly")),
		("--hostname=", .missingOptionValue("--hostname")),
		("--domainname=", .missingOptionValue("--domainname")),
		("--ulimit=", .missingOptionValue("--ulimit")),
		("--umask=", .missingOptionValue("--umask")),
		("--oom-score-adj=", .missingOptionValue("--oom-score-adj")),
		("--scheduler=", .missingOptionValue("--scheduler")),
		("--io-priority=", .missingOptionValue("--io-priority")),
		("--personality=", .missingOptionValue("--personality")),
		("--cap-set=", .missingOptionValue("--cap-set")),
		("--cpu-affinity=", .missingOptionValue("--cpu-affinity")),
	]

		for (option, expectedError) in invalidOptions {
			XCTAssertThrowsError(
				try OrlixOCIEnvironmentRunArguments([
					"run",
					option,
					"alpine:3.20",
				])
			) { error in
				XCTAssertEqual(
					error as? OrlixOCIEnvironmentRunArgumentsError,
					expectedError
				)
			}
		}
	}

	func testOCIEnvironmentRunArgumentsDerivesStorageSafeID() throws {
		let arguments = try OrlixOCIEnvironmentRunArguments([
			"run",
			"alpine:3.20",
	])

	XCTAssertEqual(arguments.image, "alpine:3.20")
	XCTAssertEqual(arguments.id, "oci-docker-io-library-alpine-3-20")
	XCTAssertEqual(arguments.platform, "linux/arm64")
	XCTAssertNil(arguments.command)
}

func testOCIEnvironmentInstallerRejectsRemoveAfterRunForTerminalSession() throws {
	let root = FileManager.default.temporaryDirectory.appendingPathComponent(
		"orlix-run-rm-terminal-\(UUID().uuidString)",
		isDirectory: true
	)
	defer { try? FileManager.default.removeItem(at: root) }
	let installer = OrlixOCIEnvironmentInstaller(
		registry: OrlixEnvironmentRegistry(
			linuxStateRoot: root.appendingPathComponent("state", isDirectory: true),
			cacheRoot: root.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: root.appendingPathComponent("scratch", isDirectory: true)
		)
	)

	XCTAssertThrowsError(
		try installer.terminalSession(arguments: [
			"orlix",
			"run",
			"--rm",
			"alpine:3.20",
		])
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIEnvironmentRunArgumentsError,
			.removeAfterRunRequiresObservedRun
		)
	}
}

func testOCIRegistryPullerWritesVerifiedImageLayoutFromIndex() async throws {
	let fileManager = FileManager.default
	let root = temporaryRegistryRoot()
	let layoutURL = root.appendingPathComponent("pulled-layout", isDirectory: true)
	defer { try? fileManager.removeItem(at: root) }

	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/alpine:3.20"
	)
	let layerData = Data("registry layer payload\n".utf8)
	let layerDigest = "sha256:\(OrlixOCIDigest.sha256Hex(layerData))"
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin"],
		    "Entrypoint": ["/bin/sh"],
			"WorkingDir": "/",
			"User": "0"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": ["\(layerDigest)"]
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": [
		    {
		      "mediaType": "application/vnd.oci.image.layer.v1.tar",
		      "digest": "\(layerDigest)",
		      "size": \(layerData.count)
		    }
		  ]
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let indexData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.index.v1+json",
		  "manifests": [
		    {
		      "mediaType": "application/vnd.oci.image.manifest.v1+json",
		      "digest": "\(manifestDigest)",
		      "size": \(manifestData.count),
		      "platform": {
		        "os": "linux",
		        "architecture": "arm64"
		      }
		    }
		  ]
		}
		""".utf8
	)
	let indexDigest = "sha256:\(OrlixOCIDigest.sha256Hex(indexData))"

	let registry = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.index.v1+json",
				"Docker-Content-Digest": indexDigest,
			],
			body: indexData
		),
		try image.manifestURL(reference: manifestDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Content-Type": "application/vnd.oci.image.manifest.v1+json"],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: configData
		),
		try image.blobURL(digest: layerDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": layerDigest],
			body: layerData
		),
	])

	let result = try await OrlixOCIRegistryPuller(fetch: registry.fetch).pull(
		image,
		to: layoutURL
	)
	XCTAssertEqual(result.manifestDigest, manifestDigest)
	XCTAssertEqual(result.configDigest, configDigest)
	XCTAssertEqual(result.layerDigests, [layerDigest])

	let pulled = try OrlixOCIImageLayoutReader().readLayout(at: layoutURL)
	XCTAssertEqual(pulled.manifestDigest, manifestDigest)
	XCTAssertEqual(pulled.configDigest, configDigest)
	XCTAssertEqual(pulled.layers.map(\.digest), [layerDigest])
	XCTAssertEqual(pulled.rootfsDiffIDs, [layerDigest])
	XCTAssertEqual(pulled.processDefaults.entrypoint, ["/bin/sh"])

	let requests = await registry.requests
	XCTAssertEqual(requests.map(\.url.absoluteString), [
		try image.manifestURL().absoluteString,
		try image.manifestURL(reference: manifestDigest).absoluteString,
		try image.blobURL(digest: configDigest).absoluteString,
		try image.blobURL(digest: layerDigest).absoluteString,
	])
	XCTAssertTrue(
		requests[0].accept.contains("application/vnd.oci.image.index.v1+json")
	)
	XCTAssertTrue(
		requests[0].accept.contains("application/vnd.oci.image.manifest.v1+json")
	)
}

func testOCIRegistryPullerSelectsArm64VariantForDefaultPlatform() async throws {
	let fileManager = FileManager.default
	let root = temporaryRegistryRoot()
	let layoutURL = root.appendingPathComponent(
		"variant-pulled-layout",
		isDirectory: true
	)
	defer { try? fileManager.removeItem(at: root) }

	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/alpine:3.20"
	)
	let configData = Data(
		#"""
		{
			"config": {
				"Entrypoint": ["/bin/sh"],
				"WorkingDir": "/"
			},
			"rootfs": {
				"type": "layers",
				"diff_ids": []
			}
		}
		"""#.utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
			"schemaVersion": 2,
			"mediaType": "application/vnd.oci.image.manifest.v1+json",
			"config": {
				"mediaType": "application/vnd.oci.image.config.v1+json",
				"digest": "\(configDigest)",
				"size": \(configData.count)
			},
			"layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let indexData = Data(
		"""
		{
			"schemaVersion": 2,
			"mediaType": "application/vnd.oci.image.index.v1+json",
			"manifests": [
				{
					"mediaType": "application/vnd.oci.image.manifest.v1+json",
					"digest": "\(manifestDigest)",
					"size": \(manifestData.count),
					"platform": {
						"os": "linux",
						"architecture": "arm64",
						"variant": "v8"
					}
				}
			]
		}
		""".utf8
	)
	let indexDigest = "sha256:\(OrlixOCIDigest.sha256Hex(indexData))"

	let registry = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.index.v1+json",
				"Docker-Content-Digest": indexDigest,
			],
			body: indexData
		),
		try image.manifestURL(reference: manifestDigest).absoluteString:
			OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Content-Type": "application/vnd.oci.image.manifest.v1+json"],
				body: manifestData
			),
		try image.blobURL(digest: configDigest).absoluteString:
			OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Docker-Content-Digest": configDigest],
				body: configData
			),
	])

	let result = try await OrlixOCIRegistryPuller(fetch: registry.fetch).pull(
		image,
		to: layoutURL
	)
	XCTAssertEqual(result.manifestDigest, manifestDigest)
	XCTAssertEqual(result.configDigest, configDigest)
	XCTAssertEqual(result.layerDigests, [])

	let pulled = try OrlixOCIImageLayoutReader().readLayout(at: layoutURL)
	XCTAssertEqual(pulled.manifestDigest, manifestDigest)
	XCTAssertEqual(pulled.configDigest, configDigest)
	XCTAssertEqual(pulled.layers.map(\.digest), [])
	XCTAssertEqual(pulled.rootfsDiffIDs, [])
	XCTAssertEqual(pulled.processDefaults.entrypoint, ["/bin/sh"])

	let requests = await registry.requests
	XCTAssertEqual(requests.map(\.url.absoluteString), [
		try image.manifestURL().absoluteString,
		try image.manifestURL(reference: manifestDigest).absoluteString,
		try image.blobURL(digest: configDigest).absoluteString,
	])
}

func testOCIRegistryPullerPullsDockerHubOfficialImageWhenLiveRegistryTestsEnabled() async throws {
#if ORLIXOS_LIVE_REGISTRY_TESTS
	let fileManager = FileManager.default
	let root = temporaryRegistryRoot()
	let layoutURL = root.appendingPathComponent(
		"docker-hub-live-layout",
		isDirectory: true
	)
	defer { try? fileManager.removeItem(at: root) }

	let image = try OrlixOCIRegistryImageReference("alpine:latest")
	XCTAssertEqual(image.registry, "docker.io")
	XCTAssertEqual(image.repository, "library/alpine")
	XCTAssertEqual(
		try image.manifestURL().absoluteString,
		"https://registry-1.docker.io/v2/library/alpine/manifests/latest"
	)

	let result = try await OrlixOCIRegistryPuller().pull(image, to: layoutURL)
	XCTAssertEqual(result.image, image)
	XCTAssertTrue(result.manifestDigest.hasPrefix("sha256:"))
	XCTAssertTrue(result.configDigest.hasPrefix("sha256:"))
	XCTAssertFalse(result.layerDigests.isEmpty)

	let pulled = try OrlixOCIImageLayoutReader().readLayout(at: layoutURL)
	XCTAssertEqual(pulled.manifestDigest, result.manifestDigest)
	XCTAssertEqual(pulled.configDigest, result.configDigest)
	XCTAssertEqual(pulled.layers.map(\.digest), result.layerDigests)
	XCTAssertFalse(pulled.rootfsDiffIDs.isEmpty)
#else
	throw XCTSkip("Build with ORLIXOS_LIVE_REGISTRY_TESTS to run live registry pull coverage.")
#endif
}

func testOCIRegistryPullerRejectsBlobDigestMismatch() async throws {
	let fileManager = FileManager.default
	let root = temporaryRegistryRoot()
	let layoutURL = root.appendingPathComponent("bad-pull-layout", isDirectory: true)
	defer { try? fileManager.removeItem(at: root) }

	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/alpine:3.20"
	)
	let expectedConfigData = Data(#"{"rootfs":{"type":"layers","diff_ids":[]}}"#.utf8)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(expectedConfigData))"
	let badConfigData = Data("tampered config\n".utf8)
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(expectedConfigData.count)
		  },
		  "layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registry = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.manifest.v1+json",
				"Docker-Content-Digest": manifestDigest,
			],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: badConfigData
		),
	])

	do {
		_ = try await OrlixOCIRegistryPuller(fetch: registry.fetch).pull(
			image,
			to: layoutURL
		)
		XCTFail("registry pull should reject tampered config blob")
	} catch {
		XCTAssertEqual(
			error as? OrlixOCIRegistryPullError,
			.responseDigestMismatch(
				url: try image.blobURL(digest: configDigest).absoluteString,
				expected: configDigest,
				actual: "sha256:\(OrlixOCIDigest.sha256Hex(badConfigData))"
			)
		)
	}
	XCTAssertFalse(fileManager.fileExists(atPath: layoutURL.path))
}

func testOCIRegistryPullerUsesBearerTokenChallenge() async throws {
	let fileManager = FileManager.default
	let root = temporaryRegistryRoot()
	let layoutURL = root.appendingPathComponent("authenticated-layout", isDirectory: true)

	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/authenticated:latest"
	)
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin"],
		    "Entrypoint": ["/bin/sh"],
		    "WorkingDir": "/"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": []
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registryFetch = RecordingOCIRegistryFetch(scriptedResponses: [
		try image.manifestURL().absoluteString: [
			OrlixOCIRegistryFetchResponse(
				statusCode: 401,
				headers: [
					"WWW-Authenticate":
						#"Bearer realm="https://auth.example.org/token",service="registry.example.org",scope="repository:library/authenticated:pull""#,
				],
				body: Data()
			),
			OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: [
					"Content-Type": "application/vnd.oci.image.manifest.v1+json",
					"Docker-Content-Digest": manifestDigest,
				],
				body: manifestData
			),
		],
		try image.blobURL(digest: configDigest).absoluteString: [
			OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Docker-Content-Digest": configDigest],
				body: configData
			),
		],
	])
	let tokenFetch = RecordingOCIRegistryFetch(
		defaultResponse: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Content-Type": "application/json"],
			body: Data(#"{ "token": "registry-token" }"#.utf8)
		)
	)
	let authorizingFetch = OrlixOCIRegistryBearerAuthorizingFetch(
		fetch: registryFetch.fetch,
		tokenFetch: tokenFetch.fetch
	)

	let result = try await OrlixOCIRegistryPuller(fetch: authorizingFetch.fetch)
		.pull(image, to: layoutURL)
	XCTAssertEqual(result.manifestDigest, manifestDigest)
	let pulled = try OrlixOCIImageLayoutReader().readLayout(at: layoutURL)
	XCTAssertEqual(pulled.manifestDigest, manifestDigest)
	XCTAssertEqual(pulled.configDigest, configDigest)

	let registryRequests = await registryFetch.requests
	let tokenRequests = await tokenFetch.requests
	XCTAssertEqual(registryRequests.count, 3)
	XCTAssertNil(registryRequests[0].authorization)
	XCTAssertEqual(registryRequests[1].authorization, "Bearer registry-token")
	XCTAssertEqual(registryRequests[2].url, try image.blobURL(digest: configDigest))
	XCTAssertEqual(tokenRequests.count, 1)
	let tokenComponents = try XCTUnwrap(
		URLComponents(url: tokenRequests[0].url, resolvingAgainstBaseURL: false)
	)
	XCTAssertEqual(tokenComponents.scheme, "https")
	XCTAssertEqual(tokenComponents.host, "auth.example.org")
	XCTAssertEqual(tokenComponents.path, "/token")
	let queryItems = tokenComponents.queryItems ?? []
	XCTAssertTrue(
		queryItems.contains(URLQueryItem(name: "service", value: "registry.example.org"))
	)
	XCTAssertTrue(
		queryItems.contains(URLQueryItem(
			name: "scope",
			value: "repository:library/authenticated:pull"
		))
	)
}

func testOCIEnvironmentInstallerInstallsRegistryImageAndBuildsSession() async throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-registry-installer-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
	defer { try? fileManager.removeItem(at: scratch) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/orlix-registry:latest"
	)
	let layerData = tarArchive(entries: [
		TarFixtureEntry(
			path: "root-marker",
			payload: Data("registry-root\n".utf8)
		),
	])
	let layerDigest = "sha256:\(OrlixOCIDigest.sha256Hex(layerData))"
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
		    "Entrypoint": ["/bin/sh"],
		    "Cmd": ["-lc", "echo registry"],
			"WorkingDir": "/",
			"User": "0",
			"StopSignal": "SIGUSR2"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": ["\(layerDigest)"]
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": [
		    {
		      "mediaType": "application/vnd.oci.image.layer.v1.tar",
		      "digest": "\(layerDigest)",
		      "size": \(layerData.count)
		    }
		  ]
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registryFetch = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.manifest.v1+json",
				"Docker-Content-Digest": manifestDigest,
			],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: configData
		),
		try image.blobURL(digest: layerDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": layerDigest],
			body: layerData
		),
	])
	let tools = OrlixOCIEnvironmentMaterializationTools(
		mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
		truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
		debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
	)
	let recorder = RecordingPublicOCIInstallerCommandRunner()
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)

	let installed = try await installer.install(
		image: image,
		id: "registry-installed-session",
		tools: tools,
		puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
		fileManager: fileManager
	) { executable, arguments in
		try recorder.run(executable: executable, arguments: arguments)
	}

	let layout = try registry.layout(forEnvironmentID: "registry-installed-session")
	let descriptor = try registry.load(environmentID: "registry-installed-session")
	XCTAssertEqual(installed.id, "registry-installed-session")
	XCTAssertEqual(installed.image, image)
	XCTAssertEqual(installed.pullResult.manifestDigest, manifestDigest)
	XCTAssertEqual(installed.stateReport.status, .created)
	XCTAssertEqual(installed.stateReport.bundle, "oci://registry.example.org/library/orlix-registry@\(manifestDigest)")
	XCTAssertEqual(descriptor.defaultCommand, ["/bin/sh", "-lc", "echo registry"])
	XCTAssertEqual(descriptor.defaultEnvironment["TERM"], "xterm-256color")
	XCTAssertTrue(fileManager.fileExists(atPath: layout.baseImageURL.path))
	XCTAssertTrue(fileManager.fileExists(atPath: layout.stateImageURL.path))
	XCTAssertEqual(recorder.executables.first, tools.truncate)
	XCTAssertEqual(recorder.executables.filter { $0 == tools.mke2fs }.count, 2)
	XCTAssertEqual(recorder.executables.filter { $0 == tools.debugfs }.count, 2)

	let session = try installer.session(
		id: "registry-installed-session",
		terminal: OrlixTerminalSession()
	)
	let rootImage = try XCTUnwrap(session.materializedRootImageForTesting)
	XCTAssertEqual(rootImage.baseImageURL, layout.baseImageURL)
	XCTAssertEqual(rootImage.stateImageURL, layout.stateImageURL)
	let requests = await registryFetch.requests
	XCTAssertEqual(requests.count, 3)
}

func testOCIEnvironmentInstallerStartsCreatedRegistryEnvironment() async throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-registry-start-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
	defer { try? fileManager.removeItem(at: scratch) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let image = try OrlixOCIRegistryImageReference(
		"registry.example.org/library/orlix-registry:latest"
	)
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
		    "Entrypoint": ["/bin/sh"],
		    "Cmd": ["-lc", "echo registry"],
		    "WorkingDir": "/",
		    "User": "0"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": []
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registryFetch = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.manifest.v1+json",
				"Docker-Content-Digest": manifestDigest,
			],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: configData
		),
	])
	let tools = OrlixOCIEnvironmentMaterializationTools(
		mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
		truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
		debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
	)
	let recorder = RecordingPublicOCIInstallerCommandRunner()
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)
	let installed = try await installer.install(
		image: image,
		id: "registry-started-session",
		tools: tools,
		puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
		fileManager: fileManager
	) { executable, arguments in
		try recorder.run(executable: executable, arguments: arguments)
	}
    let driver = try RecordingOCIRuntimeProcessObservationDriver(
        startPID: 42,
        completion: .signaled(
			OrlixOCIRuntimeProcessSignalObservation(pid: 42, signal: 12)
        )
    )

	let started = try installer.start(
		id: installed.id,
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver,
		fileManager: fileManager
	)
    let runningReport = try installer.state(id: installed.id, fileManager: fileManager)
let signaled = try installer.kill(
arguments: ["kill", installed.id],
terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
using: driver,
fileManager: fileManager
)
    let signaledReport = try installer.state(id: installed.id, fileManager: fileManager)
    let completed = try installer.wait(
        id: installed.id,
        terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver,
		fileManager: fileManager
	)
	let stoppedReport = try installer.state(id: installed.id, fileManager: fileManager)

	XCTAssertEqual(started.id, installed.id)
	XCTAssertEqual(started.stateReport.status, .running)
    XCTAssertEqual(started.stateReport.pid, 42)
    XCTAssertEqual(runningReport.status, .running)
    XCTAssertEqual(runningReport.pid, 42)
    XCTAssertEqual(signaled.id, installed.id)
XCTAssertEqual(signaled.signal, 12)
    XCTAssertEqual(signaled.stateReport.status, .running)
    XCTAssertEqual(signaled.stateReport.pid, 42)
    XCTAssertEqual(signaledReport.status, .running)
    XCTAssertEqual(signaledReport.pid, 42)
    XCTAssertEqual(completed.id, installed.id)
    XCTAssertEqual(completed.stateReport.status, .stopped)
    XCTAssertEqual(completed.stateReport.pid, 42)
XCTAssertEqual(completed.stateReport.exitStatus, 140)
    XCTAssertEqual(stoppedReport.status, .stopped)
XCTAssertEqual(stoppedReport.exitStatus, 140)
    XCTAssertEqual(driver.events, [
        "start:created:nil",
"signal:running:42:12",
        "wait:running:42",
    ])
}

func testOCIEnvironmentInstallerInstallsDockerShorthandImageStringAndBuildsSession() async throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-registry-string-installer-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
		defer { try? fileManager.removeItem(at: scratch) }

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
		let imageString = "alpine:3.20"
		let image = try OrlixOCIRegistryImageReference(imageString)
		let configData = Data(
			"""
			{
			  "config": {
			    "Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
			    "Entrypoint": ["/bin/sh"],
			    "Cmd": ["-lc", "echo string"],
			    "WorkingDir": "/",
			    "User": "0"
			  },
			  "rootfs": {
			    "type": "layers",
			    "diff_ids": []
			  }
			}
			""".utf8
		)
		let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
		let manifestData = Data(
			"""
			{
			  "schemaVersion": 2,
			  "mediaType": "application/vnd.oci.image.manifest.v1+json",
			  "config": {
			    "mediaType": "application/vnd.oci.image.config.v1+json",
			    "digest": "\(configDigest)",
			    "size": \(configData.count)
			  },
			  "layers": []
			}
			""".utf8
		)
		let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
		let registryFetch = RecordingOCIRegistryFetch(responses: [
			try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: [
					"Content-Type": "application/vnd.oci.image.manifest.v1+json",
					"Docker-Content-Digest": manifestDigest,
				],
				body: manifestData
			),
			try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Docker-Content-Digest": configDigest],
				body: configData
			),
		])
		let tools = OrlixOCIEnvironmentMaterializationTools(
			mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
			truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
			debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
		)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let recorder = RecordingPublicOCIInstallerCommandRunner()

		let result = try await installer.install(
			image: imageString,
			id: "docker-shorthand-string",
			tools: tools,
			puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
			runCommand: { executable, arguments in
				try recorder.run(executable: executable, arguments: arguments)
			}
		)

		let session = try installer.session(id: result.id)

		XCTAssertEqual(result.image, image)
		XCTAssertEqual(result.pullResult.image, image)
		XCTAssertEqual(
			try registry.load(environmentID: result.id).defaultCommand,
			["/bin/sh", "-lc", "echo string"]
		)
		let kernelCommandLine = try XCTUnwrap(session.bootConfig.kernelCommandLine)
		XCTAssertTrue(kernelCommandLine.contains("orlix.exec=/bin/sh"))
		XCTAssertTrue(kernelCommandLine.contains("orlix.argv1=-lc"))
		XCTAssertEqual(recorder.executables.map(\.lastPathComponent), [
			"orlix-truncate",
			"orlix-mke2fs",
			"orlix-debugfs",
			"orlix-truncate",
			"orlix-mke2fs",
			"orlix-debugfs",
		])
		let requests = await registryFetch.requests
	XCTAssertEqual(requests.map(\.url.absoluteString), [
		try image.manifestURL().absoluteString,
		try image.blobURL(digest: configDigest).absoluteString,
	])
}

func testOCIEnvironmentInstallerRunsOrlixRunArgumentsThroughRegistryImagePath() async throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-run-arguments-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
	defer { try? fileManager.removeItem(at: scratch) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let imageString = "alpine:3.20"
	let image = try OrlixOCIRegistryImageReference(imageString)
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
		    "Entrypoint": ["/bin/sh"],
		    "Cmd": ["-lc", "echo default"],
		    "WorkingDir": "/",
		    "User": "0"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": []
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registryFetch = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.manifest.v1+json",
				"Docker-Content-Digest": manifestDigest,
			],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: configData
		),
	])
	let tools = OrlixOCIEnvironmentMaterializationTools(
		mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
		truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
		debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
	)
	let recorder = RecordingPublicOCIInstallerCommandRunner()
	let driver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 42,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
		)
	)
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)

	let result = try await installer.run(
		arguments: [
			"orlix",
"run",
"--id",
"orlix-run-arguments",
"--publish",
"127.0.0.1:8080:80",
imageString,
"--",
"/usr/bin/env",
			"true",
		],
		tools: tools,
		puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver,
		fileManager: fileManager
	) { executable, arguments in
		try recorder.run(executable: executable, arguments: arguments)
	}

	XCTAssertEqual(result.installResult.image, image)
	XCTAssertEqual(result.runResult.startedStateReport.status, .running)
		XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
		XCTAssertEqual(result.runResult.completedStateReport.pid, 42)
		XCTAssertEqual(result.runResult.completedStateReport.exitStatus, 0)
		XCTAssertNil(result.deleteResult)
let descriptor = try registry.load(environmentID: "orlix-run-arguments")
XCTAssertEqual(descriptor.defaultCommand, ["/bin/sh", "-lc", "echo default"])
XCTAssertEqual(descriptor.publishedPorts, [
OrlixEnvironmentPublishedPort(
containerPort: 80,
proto: "tcp",
hostPort: 8080,
hostAddress: "127.0.0.1"
),
])
XCTAssertEqual(driver.events, [
		"start:created:nil",
		"wait:running:42",
	])
	XCTAssertEqual(recorder.executables.map(\.lastPathComponent), [
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
	])
	let requests = await registryFetch.requests
		XCTAssertEqual(requests.map(\.url.absoluteString), [
			try image.manifestURL().absoluteString,
			try image.blobURL(digest: configDigest).absoluteString,
		])
	}

	func testOCIEnvironmentInstallerRunArgumentsRemoveEnvironmentAfterRun() async throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-registry-installer-run-rm-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
		defer { try? fileManager.removeItem(at: scratch) }

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
		let image = try OrlixOCIRegistryImageReference("alpine:3.20")
		let configData = Data(
			"""
			{
			  "config": {
			    "Entrypoint": ["/bin/sh"],
			    "Cmd": ["-lc", "echo ephemeral"],
			    "WorkingDir": "/",
			    "User": "0"
			  },
			  "rootfs": {
			    "type": "layers",
			    "diff_ids": []
			  }
			}
			""".utf8
		)
		let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
		let manifestData = Data(
			"""
			{
			  "schemaVersion": 2,
			  "mediaType": "application/vnd.oci.image.manifest.v1+json",
			  "config": {
			    "mediaType": "application/vnd.oci.image.config.v1+json",
			    "digest": "\(configDigest)",
			    "size": \(configData.count)
			  },
			  "layers": []
			}
			""".utf8
		)
		let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
		let registryFetch = RecordingOCIRegistryFetch(responses: [
			try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: [
					"Content-Type": "application/vnd.oci.image.manifest.v1+json",
					"Docker-Content-Digest": manifestDigest,
				],
				body: manifestData
			),
			try image.blobURL(digest: configDigest).absoluteString:
				OrlixOCIRegistryFetchResponse(
					statusCode: 200,
					headers: ["Docker-Content-Digest": configDigest],
					body: configData
				),
		])
		let tools = OrlixOCIEnvironmentMaterializationTools(
			mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
			truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
			debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
		)
		let recorder = RecordingPublicOCIInstallerCommandRunner()
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 43,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 43, exitStatus: 0)
			)
		)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)

		let result = try await installer.run(
			arguments: [
				"orlix",
				"run",
				"--rm",
				"--id",
				"orlix-run-rm",
				"alpine:3.20",
			],
			tools: tools,
			puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			using: driver,
			fileManager: fileManager
		) { executable, arguments in
			try recorder.run(executable: executable, arguments: arguments)
		}

		XCTAssertEqual(result.installResult.id, "orlix-run-rm")
		XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
		XCTAssertEqual(result.deleteResult?.id, "orlix-run-rm")
		XCTAssertEqual(result.deleteResult?.lifecycleState, .deleted)
		XCTAssertThrowsError(
			try installer.state(id: "orlix-run-rm", fileManager: fileManager)
		) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleStoreError,
				.missingRecord("orlix-run-rm")
			)
		}
		XCTAssertThrowsError(try registry.load(environmentID: "orlix-run-rm"))
	}

	func testOCIEnvironmentInstallerPreparesTerminalSessionFromOrlixRunArguments() async throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-run-terminal-session-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
	defer { try? fileManager.removeItem(at: scratch) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let image = try OrlixOCIRegistryImageReference("alpine:3.20")
	let configData = Data(
		"""
		{
		  "config": {
		    "Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
		    "Entrypoint": ["/bin/sh"],
		    "Cmd": ["-lc", "echo default"],
		    "WorkingDir": "/",
		    "User": "0"
		  },
		  "rootfs": {
		    "type": "layers",
		    "diff_ids": []
		  }
		}
		""".utf8
	)
	let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
	let manifestData = Data(
		"""
		{
		  "schemaVersion": 2,
		  "mediaType": "application/vnd.oci.image.manifest.v1+json",
		  "config": {
		    "mediaType": "application/vnd.oci.image.config.v1+json",
		    "digest": "\(configDigest)",
		    "size": \(configData.count)
		  },
		  "layers": []
		}
		""".utf8
	)
	let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
	let registryFetch = RecordingOCIRegistryFetch(responses: [
		try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: [
				"Content-Type": "application/vnd.oci.image.manifest.v1+json",
				"Docker-Content-Digest": manifestDigest,
			],
			body: manifestData
		),
		try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
			statusCode: 200,
			headers: ["Docker-Content-Digest": configDigest],
			body: configData
		),
	])
	let tools = OrlixOCIEnvironmentMaterializationTools(
		mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
		truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
		debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
	)
	let recorder = RecordingPublicOCIInstallerCommandRunner()
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)

	let result = try await installer.prepareTerminalSession(
			arguments: [
				"orlix", "run", "--id", "orlix-run-terminal-session",
				"--annotation", "com.example.session=terminal",
				"alpine:3.20", "--", "/bin/sh", "-lc", "echo terminal",
			],
		tools: tools,
		puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		fileManager: fileManager
	) { executable, arguments in
		try recorder.run(executable: executable, arguments: arguments)
	}

	let rootImage = try XCTUnwrap(result.linuxSession.materializedRootImageForTesting)
	let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
		XCTAssertEqual(result.installResult.id, "orlix-run-terminal-session")
		XCTAssertEqual(result.installResult.image, image)
		XCTAssertEqual(result.installResult.stateReport.status, .created)
		XCTAssertEqual(
			result.installResult.stateReport.annotations["com.example.session"],
			"terminal"
		)
		XCTAssertTrue(commandLine.contains("orlix.exec=/bin/sh"))
	XCTAssertTrue(commandLine.contains("orlix.argv1=-lc"))
	XCTAssertTrue(commandLine.contains("orlix.argv2=echo%20terminal"))
		XCTAssertEqual(
			try registry.load(environmentID: "orlix-run-terminal-session").defaultCommand,
			["/bin/sh", "-lc", "echo default"]
		)
		XCTAssertEqual(
			try registry.load(environmentID: "orlix-run-terminal-session")
				.annotations["com.example.session"],
			"terminal"
		)
		XCTAssertEqual(recorder.executables.map(\.lastPathComponent), [
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
	])
	let requests = await registryFetch.requests
	XCTAssertEqual(requests.map(\.url.absoluteString), [
		try image.manifestURL().absoluteString,
		try image.blobURL(digest: configDigest).absoluteString,
	])

	let opened = try installer.terminalSession(
		arguments: [
			"run", "--id", "orlix-run-terminal-session",
			"alpine:3.20", "--", "/bin/sh", "-lc", "echo reopened",
		],
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		fileManager: fileManager
	)
	let reopenedRootImage = try XCTUnwrap(opened.linuxSession.materializedRootImageForTesting)
	let reopenedCommandLine = try XCTUnwrap(reopenedRootImage.bootConfig.kernelCommandLine)
	XCTAssertEqual(opened.id, "orlix-run-terminal-session")
	XCTAssertEqual(opened.image, image)
	XCTAssertEqual(opened.command, ["/bin/sh", "-lc", "echo reopened"])
	XCTAssertTrue(reopenedCommandLine.contains("orlix.exec=/bin/sh"))
	XCTAssertTrue(reopenedCommandLine.contains("orlix.argv1=-lc"))
	XCTAssertTrue(reopenedCommandLine.contains("orlix.argv2=echo%20reopened"))
	XCTAssertEqual(recorder.executables.map(\.lastPathComponent), [
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
		"orlix-truncate",
		"orlix-mke2fs",
		"orlix-debugfs",
	])
}

func testOCIEnvironmentInstallerRunsRegistryImageByInstallingThenStarting() async throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-registry-installer-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
		defer { try? fileManager.removeItem(at: scratch) }

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
		let image = try OrlixOCIRegistryImageReference(
			"registry.example.org/library/orlix-registry:latest"
		)
		let layerData = tarArchive(entries: [
			TarFixtureEntry(
				path: "root-marker",
				payload: Data("registry-root\n".utf8)
			),
		])
		let layerDigest = "sha256:\(OrlixOCIDigest.sha256Hex(layerData))"
		let configData = Data(
			"""
			{
				"config": {
					"Env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color"],
					"Entrypoint": ["/bin/sh"],
					"Cmd": ["-lc", "echo registry"],
					"WorkingDir": "/",
					"User": "0"
				},
				"rootfs": {
					"type": "layers",
					"diff_ids": ["\(layerDigest)"]
				}
			}
			""".utf8
		)
		let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
		let manifestData = Data(
			"""
			{
				"schemaVersion": 2,
				"mediaType": "application/vnd.oci.image.manifest.v1+json",
				"config": {
					"mediaType": "application/vnd.oci.image.config.v1+json",
					"digest": "\(configDigest)",
					"size": \(configData.count)
				},
				"layers": [
					{
						"mediaType": "application/vnd.oci.image.layer.v1.tar",
						"digest": "\(layerDigest)",
						"size": \(layerData.count)
					}
				]
			}
			""".utf8
		)
		let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
		let registryFetch = RecordingOCIRegistryFetch(responses: [
			try image.manifestURL().absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: [
					"Content-Type": "application/vnd.oci.image.manifest.v1+json",
					"Docker-Content-Digest": manifestDigest,
				],
				body: manifestData
			),
			try image.blobURL(digest: configDigest).absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Docker-Content-Digest": configDigest],
				body: configData
			),
			try image.blobURL(digest: layerDigest).absoluteString: OrlixOCIRegistryFetchResponse(
				statusCode: 200,
				headers: ["Docker-Content-Digest": layerDigest],
				body: layerData
			),
		])
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let tools = OrlixOCIEnvironmentMaterializationTools(
			mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
			truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
			debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
		)
		let recorder = RecordingPublicOCIInstallerCommandRunner()
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 111,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 111, exitStatus: 0)
			)
		)

    let result = try await installer.run(
        arguments: [
			"orlix",
			"run",
		"--id",
		"registry-installed-run",
		"--no-tty",
		"--terminal-size=33x120",
		"--no-new-privileges",
			"--close-fds",
			"--read-only",
			"--root-propagation",
			"shared",
			"--entrypoint",
			"/usr/bin/env",
			"--env",
			"TERM=orlix-256color",
			"--env=ORLIX_RUN=1",
			"--workdir",
			"/workspace",
			"--user",
			"1000:100",
			"--group-add",
			"44",
			"--group-add=45",
			"--cgroups-path",
			"registry-run",
			"--pids-limit",
			"64",
			"--cpu-max",
			"50000:100000",
			"--cpu-weight",
			"39",
			"--memory-max",
			"268435456",
			"--io-weight",
			"100",
			"--cgroup-unified",
			"cpu.weight=39",
			"--cgroup-unified=memory.max=268435456",
			"--tmpfs",
			"/run/cache:nosuid,nodev,noexec,size=64m,mode=0755",
			"--tmpfs=/var/tmp:ro",
			"--mount",
			"type=bind,source=orlix:documents,target=/mnt/documents,readonly,noexec",
			"--volume",
			"documents:/mnt/volume-docs:ro",
			"--device-node",
			"/dev/orlix-zero:c:1:5:0660:1000:100",
		"--device-node=/dev/orlix-pipe:p:0644",
		"--namespace",
		"pid",
		"--namespace",
		"time",
		"--namespace",
		"user",
		"--namespace-path=network=/proc/1/ns/net2",
		"--time-offset",
		"monotonic:12:34",
		"--uid-map",
		"0:501:1",
		"--gid-map=0:20:1",
		"--sysctl",
		"net.ipv4.ip_forward=1",
		"--sysctl=kernel.hostname=registry-run-host",
			"--mask",
			"/proc/kcore",
			"--mask=/sys/firmware",
			"--readonly",
			"/proc/sys",
			"--readonly=/sys",
			"--cap-set",
			"bounding=CAP_CHOWN,CAP_SETUID",
			"--cap-set",
			"permitted=CAP_CHOWN",
			"--cap-set",
			"effective=CAP_CHOWN",
			"--cap-set",
			"ambient=CAP_SETUID",
			"--ulimit",
			"nofile=32:64",
			"--ulimit=stack=8388608",
			"--umask",
			"022",
			"--oom-score-adj",
			"-250",
			"--scheduler",
			"SCHED_FIFO:2",
			"--io-priority",
			"IOPRIO_CLASS_BE:5",
			"--personality",
			"LINUX32",
			"--cpu-affinity",
			"0,2-3",
			"--hostname",
			"registry-run-host",
			"--domainname",
			"registry-run.example",
			"registry.example.org/library/orlix-registry:latest",
			"sh",
			"-lc",
            "echo registry",
        ],
        tools: tools,
        puller: OrlixOCIRegistryPuller(fetch: registryFetch.fetch),
        terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
        using: driver,
        fileManager: fileManager
		) { executable, arguments in
			try recorder.run(executable: executable, arguments: arguments)
		}
		let layout = try registry.layout(forEnvironmentID: "registry-installed-run")
		let descriptor = try registry.load(environmentID: "registry-installed-run")

		XCTAssertEqual(result.installResult.id, "registry-installed-run")
		XCTAssertEqual(result.installResult.image, image)
		XCTAssertEqual(result.installResult.pullResult.manifestDigest, manifestDigest)
		XCTAssertEqual(result.installResult.stateReport.status, .created)
		XCTAssertEqual(result.runResult.id, "registry-installed-run")
		XCTAssertEqual(result.runResult.startedStateReport.status, .running)
		XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
		XCTAssertEqual(result.runResult.completedStateReport.exitStatus, 0)
		XCTAssertNil(result.deleteResult)
		XCTAssertEqual(
			try installer.state(id: "registry-installed-run"),
			result.runResult.completedStateReport
		)
	XCTAssertEqual(
		descriptor.defaultCommand,
		["/usr/bin/env", "sh", "-lc", "echo registry"]
	)
	XCTAssertEqual(descriptor.defaultEnvironment["PATH"], "/usr/bin:/bin")
	XCTAssertEqual(descriptor.defaultEnvironment["TERM"], "orlix-256color")
	XCTAssertEqual(descriptor.defaultEnvironment["ORLIX_RUN"], "1")
	XCTAssertEqual(descriptor.defaultWorkingDirectory, "/workspace")
	XCTAssertTrue(descriptor.rootReadonly)
	XCTAssertEqual(descriptor.rootPropagation, .shared)
	XCTAssertEqual(descriptor.defaultUserID, 1000)
	XCTAssertEqual(descriptor.defaultGroupID, 100)
	XCTAssertEqual(descriptor.defaultSupplementaryGroups, [44, 45])
	XCTAssertEqual(descriptor.cgroupsPath, "/orlix/registry-run")
	XCTAssertEqual(descriptor.cgroupPidsLimit, 64)
	XCTAssertEqual(
		descriptor.cgroupCPUMax,
		OrlixEnvironmentCgroupCPUMax(
			quotaMicros: 50_000,
			periodMicros: 100_000
		)
	)
	XCTAssertEqual(descriptor.cgroupCPUWeight, 39)
	XCTAssertEqual(descriptor.cgroupMemoryMax, 268_435_456)
	XCTAssertEqual(descriptor.cgroupIOWeight, 100)
	XCTAssertEqual(
		descriptor.cgroupUnified,
		[
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "cpu.weight",
				value: "39"
			),
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "memory.max",
				value: "268435456"
			),
		]
	)
	XCTAssertEqual(
		descriptor.tmpfsMounts,
		[
			try OrlixEnvironmentTmpfsMount(
				targetPath: "/run/cache",
				readOnly: false,
				noSuid: true,
				noDev: true,
				noExec: true,
				data: "size=64m,mode=0755"
			),
			try OrlixEnvironmentTmpfsMount(
				targetPath: "/var/tmp",
				readOnly: true
			),
		]
	)
	XCTAssertEqual(
		descriptor.mounts,
		[
			try OrlixEnvironmentMount.documents(
				targetPath: "/mnt/documents",
				readOnly: true,
				noExec: true
			),
			try OrlixEnvironmentMount.documents(
				targetPath: "/mnt/volume-docs",
				readOnly: true
			),
		]
	)
	XCTAssertEqual(
		descriptor.deviceNodes,
		[
			OrlixEnvironmentDeviceNode(
				path: "/dev/orlix-null",
				type: "c",
				major: 1,
				minor: 3,
				fileMode: 0o666,
				uid: 0,
				gid: 0
			),
			OrlixEnvironmentDeviceNode(
				path: "/dev/orlix-pipe",
				type: "p",
				fileMode: 0o644,
				uid: 0,
				gid: 0
			),
			OrlixEnvironmentDeviceNode(
				path: "/dev/orlix-zero",
				type: "c",
				major: 1,
				minor: 5,
				fileMode: 0o660,
				uid: 1000,
				gid: 100
			),
		]
	)
	XCTAssertEqual(
		descriptor.namespaces,
		["cgroup", "ipc", "mount", "pid", "time", "user", "uts"]
	)
	XCTAssertEqual(
		descriptor.namespacePaths,
		[
			"network": "/proc/1/ns/net2",
		]
	)
	XCTAssertEqual(
		descriptor.timeOffsets,
		[
			OrlixEnvironmentTimeOffset(
				clock: "monotonic",
				secs: 12,
				nanosecs: 34
			),
		]
	)
	XCTAssertEqual(
		descriptor.uidMappings,
		[OrlixEnvironmentIDMapping(containerID: 0, hostID: 501, size: 1)]
	)
	XCTAssertEqual(
		descriptor.gidMappings,
		[OrlixEnvironmentIDMapping(containerID: 0, hostID: 20, size: 1)]
	)
	XCTAssertEqual(
		descriptor.sysctls,
		[
			"kernel.hostname": "registry-run-host",
			"net.ipv4.ip_forward": "1",
		]
	)
	XCTAssertEqual(descriptor.maskedPaths, ["/proc/kcore", "/sys/firmware"])
	XCTAssertEqual(descriptor.readonlyPaths, ["/proc/sys", "/sys"])
	XCTAssertEqual(
		descriptor.defaultCapabilities,
		OrlixEnvironmentCapabilities(
			bounding: ["CAP_CHOWN", "CAP_SETUID"],
			permitted: ["CAP_CHOWN"],
			effective: ["CAP_CHOWN"],
			ambient: ["CAP_SETUID"]
		)
	)
	XCTAssertEqual(descriptor.defaultTerminal, false)
	XCTAssertEqual(descriptor.defaultTerminalRows, 33)
	XCTAssertEqual(descriptor.defaultTerminalColumns, 120)
	XCTAssertTrue(descriptor.defaultNoNewPrivileges)
	XCTAssertTrue(descriptor.defaultCloseAdditionalFds)
	XCTAssertEqual(descriptor.defaultUmask, 0o022)
	XCTAssertEqual(descriptor.defaultOOMScoreAdjustment, -250)
	XCTAssertEqual(
		descriptor.defaultScheduler,
		OrlixEnvironmentScheduler(policy: "SCHED_FIFO", priority: 2)
	)
	XCTAssertEqual(
		descriptor.defaultIOPriority,
		OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_BE", priority: 5)
	)
	XCTAssertEqual(descriptor.defaultPersonalityDomain, "LINUX32")
	XCTAssertEqual(
		descriptor.defaultCPUAffinity,
		OrlixEnvironmentCPUAffinity(mask: "0,2-3")
	)
	XCTAssertEqual(
		descriptor.defaultRlimits,
		[
			OrlixEnvironmentRlimit(
				type: "RLIMIT_NOFILE",
				soft: 32,
				hard: 64
			),
			OrlixEnvironmentRlimit(
				type: "RLIMIT_STACK",
				soft: 8_388_608,
				hard: 8_388_608
			),
		]
	)
	XCTAssertEqual(descriptor.hostname, "registry-run-host")
	XCTAssertEqual(descriptor.domainname, "registry-run.example")
	XCTAssertEqual(
		driver.startCommands,
		[["/usr/bin/env", "sh", "-lc", "echo registry"]]
    )
		XCTAssertEqual(recorder.executables.first, tools.truncate)
		XCTAssertEqual(recorder.executables.filter { $0 == tools.mke2fs }.count, 2)
		XCTAssertEqual(recorder.executables.filter { $0 == tools.debugfs }.count, 2)
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:111",
			]
		)
		XCTAssertTrue(fileManager.fileExists(atPath: layout.rootDirectory.path))
		let requests = await registryFetch.requests
		XCTAssertEqual(requests.count, 3)

		let deleted = try installer.delete(id: "registry-installed-run")
		XCTAssertEqual(deleted.lifecycleState, .deleted)
		XCTAssertFalse(fileManager.fileExists(atPath: layout.rootDirectory.path))
	}

	func testOCIRuntimeBundleRejectsSymlinkRootfsEscapingBundle() throws {
		let fileManager = FileManager.default
	let bundleURL = fileManager.temporaryDirectory
		.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
	let outsideRootfsURL = fileManager.temporaryDirectory
		.appendingPathComponent("orlix-oci-rootfs-outside-\(UUID().uuidString)", isDirectory: true)
	defer {
		try? fileManager.removeItem(at: bundleURL)
		try? fileManager.removeItem(at: outsideRootfsURL)
	}

	try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
	try fileManager.createDirectory(at: outsideRootfsURL, withIntermediateDirectories: true)
	try fileManager.createSymbolicLink(
		at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
		withDestinationURL: outsideRootfsURL
	)
	try minimalOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	XCTAssertThrowsError(try OrlixOCIRuntimeBundle.load(from: bundleURL)) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeBundleError,
			.rootfsEscapesBundle("rootfs")
		)
	}
}

func testOCIRuntimeBundleRejectsUnsafeEnvironmentIDs() throws {
	let fileManager = FileManager.default
	let bundleURL = fileManager.temporaryDirectory
		.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try fileManager.createDirectory(
			at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
			withIntermediateDirectories: true
		)
		try minimalOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)

		for unsafeID in ["", "..", ".hidden", "a..b", "a/b", #"a\b"#, "a\u{0}b"] {
			XCTAssertThrowsError(
				try bundle.sessionDescriptor(
					id: unsafeID,
					rootMount: .defaultOverlay
				),
				unsafeID
			) { error in
				XCTAssertEqual(
					error as? OrlixEnvironmentStorageLayoutError,
					.invalidEnvironmentID(unsafeID)
				)
			}

			XCTAssertThrowsError(
				try bundle.importPlan(
					id: unsafeID,
					rootMount: .defaultOverlay,
					storagePolicy: .current,
					fileManager: fileManager
				),
				unsafeID
			) { error in
				XCTAssertEqual(
					error as? OrlixEnvironmentStorageLayoutError,
					.invalidEnvironmentID(unsafeID)
				)
			}
		}
	}

	func testOCIRuntimeBundleResolvesConfiguredRelativeRootPath() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let configuredRootfsURL = bundleURL.appendingPathComponent("roots/alpine", isDirectory: true)
		try fileManager.createDirectory(at: configuredRootfsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig(rootPath: "roots/alpine").write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)

		XCTAssertEqual(bundle.rootfsURL, configuredRootfsURL)
		XCTAssertEqual(bundle.config.rootPath, "roots/alpine")
	}

	func testOCIRuntimeBundleRejectsRootPathEscapingBundle() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig(rootPath: "../escaped-root").write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		XCTAssertThrowsError(try OrlixOCIRuntimeBundle.load(from: bundleURL)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeBundleError,
				.rootfsEscapesBundle("../escaped-root")
			)
		}
	}

	func testOCIRuntimeBundleCarriesReadonlyRootIntoEnvironmentDescriptors() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try #"{"ociVersion":"1.1.0","root":{"path":"rootfs","readonly":true},"process":{"args":["/usr/bin/env","sh","-lc"],"env":["PATH=/usr/bin:/bin"],"cwd":"/work","user":{"uid":1000,"gid":1000}}}"#
			.data(using: .utf8)!.write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)
		let session = try bundle.sessionDescriptor(
			id: "readonly-root",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		let importPlan = try bundle.importPlan(
			id: "readonly-root",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		XCTAssertTrue(bundle.config.rootReadonly)
		XCTAssertTrue(session.environment.rootReadonly)
		XCTAssertTrue(importPlan.environment.rootReadonly)
		XCTAssertEqual(importPlan.environment.rootMount, OrlixEnvironmentRootMount.defaultOverlay)
	}

	func testOCIRuntimeBundleCarriesProcessDefaultsIntoSessionDescriptor() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try """
		{
		  "ociVersion": "1.1.0",
		  "root": { "path": "rootfs" },
		  "process": {
		    "args": ["/usr/bin/env", "sh", "-lc", "id"],
		    "env": ["PATH=/usr/bin:/bin", "TERM=xterm-256color", "ORLIX_MODE=oci"],
		    "noNewPrivileges": true,
		    "closeAdditionalFds": true,
		    "oomScoreAdj": -500,
		    "scheduler": { "policy": "SCHED_FIFO", "priority": 1 },
		    "ioPriority": { "class": "IOPRIO_CLASS_BE", "priority": 4 },
		    "execCPUAffinity": { "initial": "0", "final": "0-1" },
		    "capabilities": {
		      "bounding": ["CAP_CHOWN", "CAP_SETUID"],
		      "permitted": ["CAP_CHOWN"],
		      "inheritable": ["CAP_SETUID"],
		      "effective": ["CAP_CHOWN"],
		      "ambient": ["CAP_SETUID"]
		    },
		    "cwd": "/work",
		    "user": { "uid": 1000, "gid": 1001, "additionalGids": [44, 45], "umask": 18 }
		  }
		}
		""".data(using: .utf8)!.write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)
		let session = try bundle.sessionDescriptor(
			id: "process-defaults",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		let importPlan = try bundle.importPlan(
			id: "process-defaults",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		XCTAssertEqual(session.environment.defaultCommand, ["/usr/bin/env", "sh", "-lc", "id"])
		XCTAssertEqual(session.environment.defaultEnvironment["PATH"], "/usr/bin:/bin")
		XCTAssertEqual(session.environment.defaultEnvironment["TERM"], "xterm-256color")
		XCTAssertEqual(session.environment.defaultEnvironment["ORLIX_MODE"], "oci")
		XCTAssertEqual(session.environment.defaultWorkingDirectory, "/work")
		XCTAssertEqual(session.environment.defaultUserID, 1000)
		XCTAssertEqual(session.environment.defaultGroupID, 1001)
		XCTAssertEqual(session.environment.defaultSupplementaryGroups, [44, 45])
		XCTAssertEqual(
			session.environment.defaultCapabilities,
			OrlixEnvironmentCapabilities(
				bounding: ["CAP_CHOWN", "CAP_SETUID"],
				permitted: ["CAP_CHOWN"],
				inheritable: ["CAP_SETUID"],
				effective: ["CAP_CHOWN"],
				ambient: ["CAP_SETUID"]
			)
		)
		XCTAssertTrue(session.environment.defaultNoNewPrivileges)
		XCTAssertTrue(session.environment.defaultCloseAdditionalFds)
		XCTAssertEqual(session.environment.defaultOOMScoreAdjustment, -500)
		XCTAssertEqual(
			session.environment.defaultScheduler,
			OrlixEnvironmentScheduler(policy: "SCHED_FIFO", priority: 1)
		)
		XCTAssertEqual(
			session.environment.defaultIOPriority,
			OrlixEnvironmentIOPriority(class: "IOPRIO_CLASS_BE", priority: 4)
		)
		XCTAssertEqual(
			session.environment.defaultCPUAffinity,
			OrlixEnvironmentCPUAffinity(mask: "0-1")
		)
		XCTAssertEqual(session.environment.defaultUmask, 18)

		XCTAssertEqual(importPlan.environment.defaultCommand, session.environment.defaultCommand)
		XCTAssertEqual(importPlan.environment.defaultEnvironment, session.environment.defaultEnvironment)
		XCTAssertEqual(importPlan.environment.defaultWorkingDirectory, session.environment.defaultWorkingDirectory)
		XCTAssertEqual(importPlan.environment.defaultUserID, session.environment.defaultUserID)
		XCTAssertEqual(importPlan.environment.defaultGroupID, session.environment.defaultGroupID)
		XCTAssertEqual(
			importPlan.environment.defaultSupplementaryGroups,
			session.environment.defaultSupplementaryGroups
		)
		XCTAssertEqual(
			importPlan.environment.defaultCapabilities,
			session.environment.defaultCapabilities
		)
		XCTAssertEqual(
			importPlan.environment.defaultNoNewPrivileges,
			session.environment.defaultNoNewPrivileges
		)
		XCTAssertEqual(
			importPlan.environment.defaultCloseAdditionalFds,
			session.environment.defaultCloseAdditionalFds
		)
		XCTAssertEqual(
			importPlan.environment.defaultOOMScoreAdjustment,
			session.environment.defaultOOMScoreAdjustment
		)
		XCTAssertEqual(
			importPlan.environment.defaultScheduler,
			session.environment.defaultScheduler
		)
		XCTAssertEqual(
			importPlan.environment.defaultIOPriority,
			session.environment.defaultIOPriority
		)
		XCTAssertEqual(
			importPlan.environment.defaultCPUAffinity,
			session.environment.defaultCPUAffinity
		)
		XCTAssertEqual(importPlan.environment.defaultUmask, session.environment.defaultUmask)
	}

	func testOrlixOSBuildsSessionFromOCIRuntimeBundle() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try fileManager.createDirectory(
			at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
			withIntermediateDirectories: true
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let registry = try OrlixEnvironmentRegistry()
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeBundle: try OrlixOCIRuntimeBundle.load(from: bundleURL),
			id: "bundle-session",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)

		let savedEnvironment = try registry.load(environmentID: "bundle-session")
		XCTAssertEqual(savedEnvironment.id, "bundle-session")

		XCTAssertNil(linuxSession.materializedRootImageForTesting)
		let commandLine = try XCTUnwrap(linuxSession.bootConfig.kernelCommandLine)
		XCTAssertTrue(commandLine.contains("orlix.exec=/usr/bin/env"), commandLine)
		XCTAssertTrue(commandLine.contains("orlix.argv0=/usr/bin/env"), commandLine)
		XCTAssertTrue(commandLine.contains("orlix.argv1=sh"), commandLine)
		XCTAssertTrue(commandLine.contains("orlix.argv2=-lc"), commandLine)
		XCTAssertTrue(commandLine.contains("orlix.uid=1000"), commandLine)
		XCTAssertTrue(commandLine.contains("orlix.gid=1000"), commandLine)
	}

	func testOCIRuntimeBundleImportPlanUsesBundleRootfsForMaterialization() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "hello from bundle rootfs\n".write(
			to: rootfsURL.appendingPathComponent("etc-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let bundle = try OrlixOCIRuntimeBundle.load(from: bundleURL)
		let importPlan = try bundle.importPlan(
			id: "bundle-import",
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		XCTAssertEqual(importPlan.bundle, bundle)
		XCTAssertEqual(importPlan.environment.id, "bundle-import")
		XCTAssertEqual(importPlan.storageLayout.environmentID, "bundle-import")
		XCTAssertEqual(importPlan.materializationPlan.stagingRootDirectory, rootfsURL)
		XCTAssertEqual(importPlan.materializationPlan.baseImageURL, importPlan.storageLayout.baseImageURL)
		XCTAssertEqual(importPlan.materializationPlan.stateImageURL, importPlan.storageLayout.stateImageURL)
		XCTAssertTrue(importPlan.materializationPlan.baseTreeDirectory.path.hasPrefix(
			importPlan.storageLayout.importScratchDirectory.path
		))
	}

	func testOCIRuntimeBundleImportPlanSavesDescriptorAndEmitsMaterializationCommands() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("bin-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let registry = try OrlixEnvironmentRegistry()
		let importPlan = try OrlixOCIRuntimeBundle
			.load(from: bundleURL)
			.importPlan(
				id: "bundle-materialize",
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)

		try importPlan.saveEnvironment(to: registry)
		let savedEnvironment = try registry.load(environmentID: "bundle-materialize")
		XCTAssertEqual(savedEnvironment, importPlan.environment)

		let commands = try importPlan.materializationCommands(
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs"
		)
		let expectedCommands = try importPlan.materializationPlan.commands(
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs"
		)
		XCTAssertEqual(commands, expectedCommands)
		XCTAssertFalse(commands.isEmpty)
	}

	func testOCIRuntimeBundleImportPlanPreparesRootfsMaterializationInputs() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		let binURL = rootfsURL.appendingPathComponent("bin", isDirectory: true)
		try fileManager.createDirectory(at: binURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try fileManager.createSymbolicLink(
			at: binURL.appendingPathComponent("sh"),
			withDestinationURL: URL(fileURLWithPath: "/usr/bin/env")
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let importPlan = try OrlixOCIRuntimeBundle
			.load(from: bundleURL)
			.importPlan(
				id: "bundle-prepared-root",
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)

		try importPlan.prepareMaterializationInputs()

		let preparedMarker = importPlan.materializationPlan.baseTreeDirectory
			.appendingPathComponent("root-marker")
		let preparedShell = importPlan.materializationPlan.baseTreeDirectory
			.appendingPathComponent("bin/sh")
		let upperDirectory = importPlan.materializationPlan.stateTreeDirectory
			.appendingPathComponent("upper", isDirectory: true)
		let workDirectory = importPlan.materializationPlan.stateTreeDirectory
			.appendingPathComponent("work", isDirectory: true)

		XCTAssertEqual(
			try String(contentsOf: preparedMarker, encoding: .utf8),
			"bundle-root\n"
		)
		XCTAssertEqual(
			try fileManager.destinationOfSymbolicLink(atPath: preparedShell.path),
			"/usr/bin/env"
		)
		XCTAssertTrue(fileManager.fileExists(atPath: upperDirectory.path))
		XCTAssertTrue(fileManager.fileExists(atPath: workDirectory.path))
	}

	func testOCIRuntimeBundleImportPlanMaterializesPreparedRootfsWithRunner() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		let etcURL = rootfsURL.appendingPathComponent("etc", isDirectory: true)
		try fileManager.createDirectory(at: etcURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try "ID=orlix\n".write(
			to: etcURL.appendingPathComponent("os-release"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let importPlan = try OrlixOCIRuntimeBundle
			.load(from: bundleURL)
			.importPlan(
				id: "bundle-materialized-root",
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)
		let runner = RecordingMaterializationCommandRunner()

		let result = try importPlan.materialize(
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs",
			runner: runner
		)

		let expectedCommands = try importPlan.materializationCommands(
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs"
		)
		XCTAssertEqual(result.commands, expectedCommands)
		XCTAssertEqual(runner.commands, expectedCommands)
		XCTAssertEqual(expectedCommands.count, 6)
		XCTAssertEqual(
			try String(
				contentsOf: importPlan.materializationPlan.baseTreeDirectory
					.appendingPathComponent("root-marker"),
				encoding: .utf8
			),
			"bundle-root\n"
		)
		XCTAssertTrue(
			fileManager.fileExists(atPath: importPlan.materializationPlan.baseMetadataCommandsURL.path)
		)
		XCTAssertTrue(
			try String(
				contentsOf: importPlan.materializationPlan.stateMetadataCommandsURL,
				encoding: .utf8
			)
			.contains("set_inode_field /upper mode 040755")
		)
	}

	func testOCIRuntimeBundleImportPlanBuildsMaterializedLinuxSessionWhenImagesExist() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let registry = try OrlixEnvironmentRegistry()
		let importPlan = try OrlixOCIRuntimeBundle
			.load(from: bundleURL)
			.importPlan(
				id: "bundle-bound-root",
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)
		try fileManager.createDirectory(
			at: importPlan.storageLayout.rootDirectory,
			withIntermediateDirectories: true
		)
		try Data("base".utf8).write(to: importPlan.storageLayout.baseImageURL)
		try Data("state".utf8).write(to: importPlan.storageLayout.stateImageURL)

		let linuxSession = try importPlan.linuxSession(
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let materializedRoot = try XCTUnwrap(linuxSession.materializedRootImageForTesting)
		XCTAssertEqual(materializedRoot.environmentID, "bundle-bound-root")
		XCTAssertEqual(materializedRoot.baseImageURL, importPlan.storageLayout.baseImageURL)
		XCTAssertEqual(materializedRoot.stateImageURL, importPlan.storageLayout.stateImageURL)
		XCTAssertEqual(materializedRoot.bootConfig.rootImageIdentifier, importPlan.environment.rootImageIdentifier)
		XCTAssertTrue(try registry.load(environmentID: "bundle-bound-root") == importPlan.environment)
	}

	func testOCIRuntimeBundleImportPlanReportsMaterializationToolchainReadiness() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		let toolsURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-materialization-tools-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: toolsURL) }

		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try fileManager.createDirectory(at: toolsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		for executableName in ["orlix-mke2fs", "orlix-truncate", "orlix-debugfs"] {
			let executableURL = toolsURL.appendingPathComponent(executableName)
			try "#!/bin/sh\nexit 0\n".write(
				to: executableURL,
				atomically: true,
				encoding: .utf8
			)
			try fileManager.setAttributes(
				[.posixPermissions: NSNumber(value: Int16(0o755))],
				ofItemAtPath: executableURL.path
			)
		}

		let importPlan = try OrlixOCIRuntimeBundle
			.load(from: bundleURL)
			.importPlan(
				id: "bundle-toolchain-ready",
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)

		let ready = try importPlan.materializationToolchainCheck(
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs",
			searchPath: [toolsURL]
		)
		XCTAssertTrue(ready.isReady)
		XCTAssertEqual(ready.missingExecutables, [])
		XCTAssertEqual(ready.requiredExecutables, [
			"orlix-mke2fs",
			"orlix-truncate",
			"orlix-debugfs"
		])
		XCTAssertEqual(
			ready.resolvedExecutables["orlix-mke2fs"],
			toolsURL.appendingPathComponent("orlix-mke2fs")
		)
		XCTAssertFalse(ready.commands.isEmpty)

		let missing = try importPlan.materializationToolchainCheck(
			mke2fsExecutable: "missing-mke2fs",
			truncateExecutable: "missing-truncate",
			debugfsExecutable: "missing-debugfs",
			searchPath: [toolsURL]
		)
		XCTAssertFalse(missing.isReady)
		XCTAssertEqual(missing.missingExecutables, [
			"missing-mke2fs",
			"missing-truncate",
			"missing-debugfs"
		])
		XCTAssertFalse(missing.commands.isEmpty)
	}

	func testOCIRuntimeBundleRejectsMissingConfig() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(
			at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
			withIntermediateDirectories: true
		)

		XCTAssertThrowsError(try OrlixOCIRuntimeBundle.load(from: bundleURL)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeBundleError,
				.missingConfig(bundleURL.appendingPathComponent("config.json").path)
			)
		}
	}

	func testOCIRuntimeBundleRejectsMissingRootfs() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try minimalOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		XCTAssertThrowsError(try OrlixOCIRuntimeBundle.load(from: bundleURL)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeBundleError,
				.missingRootfs(bundleURL.appendingPathComponent("rootfs", isDirectory: true).path)
			)
		}
	}

	func testOCIRuntimeBundleRejectsFileRootfs() throws {
		let fileManager = FileManager.default
		let bundleURL = fileManager.temporaryDirectory
			.appendingPathComponent("orlix-oci-bundle-\(UUID().uuidString)", isDirectory: true)
		defer { try? fileManager.removeItem(at: bundleURL) }

		try fileManager.createDirectory(at: bundleURL, withIntermediateDirectories: true)
		try minimalOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		try Data("not a directory".utf8).write(
			to: bundleURL.appendingPathComponent("rootfs")
		)

		XCTAssertThrowsError(try OrlixOCIRuntimeBundle.load(from: bundleURL)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeBundleError,
				.rootfsIsNotDirectory(bundleURL.appendingPathComponent("rootfs").path)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerFollowsCreateStartKillDeleteOrder() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)

		XCTAssertEqual(configured.record.state, .configured)
		XCTAssertNil(configured.record.pid)

	let created = try configured.create()
	XCTAssertEqual(created.record.state, .created)
	XCTAssertNil(created.record.pid)
	let createdReport = try created.stateReport()
	XCTAssertEqual(createdReport.status, .created)
	XCTAssertNil(createdReport.pid)
	XCTAssertEqual(createdReport.bundle, "/bundles/oci-demo")

	let running = try created.start(pid: 42)
		XCTAssertEqual(running.record.state, .running)
		XCTAssertEqual(running.record.pid, 42)

		let signaled = try running.kill(signal: 15)
		XCTAssertEqual(signaled.record.state, .running)
		XCTAssertEqual(signaled.record.pid, 42)
		XCTAssertNil(signaled.record.exitStatus)

		let stopped = try signaled.exit(
			observedSignal: OrlixOCIRuntimeProcessSignalObservation(
				pid: 42,
				signal: 15
			)
		)
		XCTAssertEqual(stopped.record.state, .stopped)
		XCTAssertEqual(stopped.record.pid, 42)
		XCTAssertEqual(stopped.record.exitStatus, 143)

		let deleted = try stopped.delete()
		XCTAssertEqual(deleted.record.state, .deleted)
		XCTAssertEqual(deleted.record.exitStatus, 143)
	}

	func testOCIRuntimeLifecycleControllerRejectsInvalidTransitions() throws {
		let configured = OrlixOCIRuntimeLifecycleController(
			config: try OrlixOCIRuntimeConfigParser().parse(minimalOCIRuntimeConfig()),
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)

		XCTAssertThrowsError(try configured.start(pid: 42)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .configured, action: .start)
			)
		}

		let created = try configured.create()
		XCTAssertThrowsError(try created.kill(signal: 15)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .created, action: .kill)
			)
		}

		let running = try created.start(pid: 42)
		XCTAssertThrowsError(try running.delete()) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .running, action: .delete)
			)
		}
		XCTAssertThrowsError(try running.kill(signal: 0)) { error in
			XCTAssertEqual(error as? OrlixOCIRuntimeLifecycleError, .invalidSignal(0))
		}
		XCTAssertThrowsError(try running.kill(signal: 128)) { error in
			XCTAssertEqual(error as? OrlixOCIRuntimeLifecycleError, .invalidSignal(128))
		}

		let deleted = try configured.delete()
		XCTAssertThrowsError(try deleted.create()) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .deleted, action: .create)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRecordsNormalProcessExit() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let running = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		.create()
		.start(pid: 42)

		let stopped = try running.exit(exitStatus: 7)

		XCTAssertEqual(stopped.record.state, .stopped)
		XCTAssertEqual(stopped.record.pid, 42)
		XCTAssertEqual(stopped.record.exitStatus, 7)

		let report = try stopped.stateReport()
		XCTAssertEqual(report.status, .stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 7)

		XCTAssertThrowsError(try stopped.exit(exitStatus: 0)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .stopped, action: .exit)
			)
		}
	}

	func testOCIRuntimeLifecycleStorePersistsStateReports() throws {
		let fileManager = FileManager.default
		let root = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-lifecycle-store-\(UUID().uuidString)",
			isDirectory: true
		)
		defer { try? fileManager.removeItem(at: root) }

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: root.appendingPathComponent("Application Support/Orlix"),
			cacheRoot: root.appendingPathComponent("Caches/Orlix"),
			scratchRoot: root.appendingPathComponent("tmp/Orlix")
		)
		let store = OrlixOCIRuntimeLifecycleStore(registry: registry)
		let config = try OrlixOCIRuntimeConfigParser().parse(
			Data(
				"""
				{
					"ociVersion": "1.1.0",
					"annotations": {
						"org.opencontainers.image.ref.name": "orlix-demo"
					},
					"process": {
						"args": ["/bin/sh"],
						"cwd": "/"
					}
				}
				""".utf8
			)
		)
		let running = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-lifecycle-store",
			bundlePath: "/bundles/oci-lifecycle-store"
		)
		.create()
		.start(pid: 4242)

		try store.save(running)

		let runningReport = try store.stateReport(id: "oci-lifecycle-store")
		XCTAssertEqual(runningReport.ociVersion, "1.1.0")
		XCTAssertEqual(runningReport.id, "oci-lifecycle-store")
		XCTAssertEqual(runningReport.status, .running)
		XCTAssertEqual(runningReport.pid, 4242)
		XCTAssertEqual(runningReport.bundle, "/bundles/oci-lifecycle-store")
		XCTAssertEqual(
			runningReport.annotations["org.opencontainers.image.ref.name"],
			"orlix-demo"
		)
		XCTAssertNil(runningReport.exitStatus)

		let stopped = try running.exit(exitStatus: 9)
		try store.save(stopped)

		let stoppedReport = try store.stateReport(id: "oci-lifecycle-store")
		XCTAssertEqual(stoppedReport.status, .stopped)
		XCTAssertEqual(stoppedReport.pid, 4242)
		XCTAssertEqual(stoppedReport.exitStatus, 9)

		try store.delete(id: "oci-lifecycle-store")
		XCTAssertThrowsError(try store.load(id: "oci-lifecycle-store")) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleStoreError,
				.missingRecord("oci-lifecycle-store")
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRecordsObservedLinuxProcessExit() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let running = try configured.create().start(pid: 42)
		let observedExit = try OrlixOCIRuntimeProcessExitObservation(
			pid: 42,
			exitStatus: 7
		)
		let stopped = try running.exit(observedProcess: observedExit)
		let report = try stopped.stateReport()

		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 7)
	}

	func testOCIRuntimeLifecycleControllerRejectsMismatchedObservedLinuxProcessExitPID() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let running = try configured.create().start(pid: 42)
		let observedExit = try OrlixOCIRuntimeProcessExitObservation(
			pid: 43,
			exitStatus: 0
		)

		XCTAssertThrowsError(try running.exit(observedProcess: observedExit)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.processPIDMismatch(expected: 42, observed: 43)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRecordsObservedLinuxSignalTermination() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let running = try configured.create().start(pid: 42)
		let stopped = try running.exit(
			observedSignal: OrlixOCIRuntimeProcessSignalObservation(
				pid: 42,
				signal: 15
			)
		)
		let report = try stopped.stateReport()

		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 143)
	}

	func testOCIRuntimeLifecycleControllerRejectsMismatchedObservedLinuxSignalPID() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let running = try configured.create().start(pid: 42)
		let observedSignal = try OrlixOCIRuntimeProcessSignalObservation(
			pid: 43,
			signal: 15
		)

		XCTAssertThrowsError(try running.exit(observedSignal: observedSignal)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.processPIDMismatch(expected: 42, observed: 43)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRejectsInvalidObservedLinuxSignal() throws {
		XCTAssertThrowsError(try OrlixOCIRuntimeProcessSignalObservation(pid: 42, signal: 0)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidSignal(0)
			)
		}

		XCTAssertThrowsError(try OrlixOCIRuntimeProcessSignalObservation(pid: 42, signal: 128)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidSignal(128)
			)
		}
	}

	func testOCIRuntimeProcessHandleBindsCreatedAndRunningSessionDescriptors() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let created = try configured.create()
		let createdProcess = try OrlixOCIRuntimeProcessHandle(
			lifecycle: created,
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		XCTAssertEqual(createdProcess.lifecycle.record.state, .created)
		XCTAssertEqual(createdProcess.sessionDescriptor.lifecycleState, .created)

		let runningProcess = try createdProcess.start(
			observedProcess: OrlixOCIRuntimeProcessStartObservation(pid: 42)
		)
		XCTAssertEqual(runningProcess.lifecycle.record.state, .running)
		XCTAssertEqual(runningProcess.lifecycle.record.pid, 42)
		XCTAssertEqual(runningProcess.sessionDescriptor.lifecycleState, .running)
		XCTAssertEqual(runningProcess.sessionDescriptor.id, "oci-demo")
	}

	func testOCIRuntimeProcessHandleRejectsInvalidObservedLinuxStartPID() throws {
		XCTAssertThrowsError(try OrlixOCIRuntimeProcessStartObservation(pid: 0)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidPID(0)
			)
		}

		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let createdProcess = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		XCTAssertThrowsError(try createdProcess.start(observedPID: -1)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidPID(-1)
			)
		}
	}

	func testOCIRuntimeProcessHandleKeepsSessionRunningUntilObservedCompletion() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let createdProcess = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		let runningProcess = try createdProcess.start(observedPID: 42)
		let signaledProcess = try runningProcess.kill(signal: 15)

		XCTAssertEqual(signaledProcess.lifecycle.record.state, .running)
		XCTAssertEqual(signaledProcess.sessionDescriptor.lifecycleState, .running)
		XCTAssertNil(signaledProcess.lifecycle.record.exitStatus)

		let completedProcess = try signaledProcess.exit(
			observedSignal: OrlixOCIRuntimeProcessSignalObservation(
				pid: 42,
				signal: 15
			)
		)
		let report = try completedProcess.stateReport()

		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 143)
		XCTAssertThrowsError(
			try OrlixOCIRuntimeProcessHandle(
				lifecycle: completedProcess.lifecycle,
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)
		) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.stopped)
			)
		}
	}

	func testOCIRuntimeProcessHandleRejectsUnmaterializableLifecycleStates() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)

		XCTAssertThrowsError(
			try OrlixOCIRuntimeProcessHandle(
				lifecycle: configured,
				rootMount: OrlixEnvironmentRootMount.defaultOverlay
			)
		) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.configured)
			)
		}
	}

	func testOCIRuntimeProcessHandleExposesDeleteTransition() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let createdProcess = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)

		let deletedCreatedProcess = try createdProcess.delete()
		XCTAssertEqual(deletedCreatedProcess.lifecycle.record.state, .deleted)
		XCTAssertNil(deletedCreatedProcess.lifecycle.record.pid)
		XCTAssertNil(deletedCreatedProcess.lifecycle.record.exitStatus)

		let runningProcess = try createdProcess.start(observedPID: 42)
		XCTAssertThrowsError(try runningProcess.delete()) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .running, action: .delete)
			)
		}

		let completedProcess = try runningProcess.exit(
			observedProcess: OrlixOCIRuntimeProcessExitObservation(
				pid: 42,
				exitStatus: 0
			)
		)
		let deletedCompletedProcess = try completedProcess.delete()
		XCTAssertEqual(deletedCompletedProcess.lifecycle.record.state, .deleted)
		XCTAssertEqual(deletedCompletedProcess.lifecycle.record.pid, 42)
		XCTAssertEqual(deletedCompletedProcess.lifecycle.record.exitStatus, 0)
	}

	func testOCIRuntimeProcessSessionBindsHandleToLinuxSession() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-session-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: cacheRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: scratchRoot,
			withIntermediateDirectories: true
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		)

		XCTAssertEqual(processSession.processHandle.lifecycle.record.state, .created)
		XCTAssertEqual(processSession.processHandle.sessionDescriptor.lifecycleState, .created)
		XCTAssertNotNil(processSession.linuxSession.materializedRootImageForTesting)
	}

	func testOCIRuntimeProcessSessionInitializerAttachesLifecycleStore() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-session-store-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(at: cacheRoot, withIntermediateDirectories: true)
		try fileManager.createDirectory(at: scratchRoot, withIntermediateDirectories: true)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let lifecycle = try OrlixOCIRuntimeLifecycleController(
			config: try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig()),
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		).create()

		let processSession = try OrlixOCIRuntimeProcessSession(
			lifecycle: lifecycle,
			rootMount: OrlixEnvironmentRootMount.defaultOverlay,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)

		let store = try XCTUnwrap(processSession.lifecycleStore)
		let report = try store.stateReport(id: "oci-demo")
		XCTAssertEqual(processSession.processHandle.lifecycle.record.state, .created)
		XCTAssertNotNil(processSession.linuxSession.materializedRootImageForTesting)
		XCTAssertEqual(report.status, .created)
		XCTAssertNil(report.pid)
		XCTAssertEqual(report.bundle, "/bundles/oci-demo")
		XCTAssertTrue(
			fileManager.fileExists(
				atPath: try registry.descriptorURL(forEnvironmentID: "oci-demo").path
			)
	)
}

func testOCIEnvironmentInstallerStateArgumentsReturnLifecycleReport() throws {
	let root = temporaryRegistryRoot()
	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: root.appendingPathComponent("state", isDirectory: true),
		cacheRoot: root.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: root.appendingPathComponent("scratch", isDirectory: true)
	)
	let store = OrlixOCIRuntimeLifecycleStore(registry: registry)
	let snapshot = OrlixOCIRuntimeLifecycleSnapshot(
		record: OrlixOCIRuntimeLifecycleRecord(
			id: "oci-state-args",
			bundlePath: "oci://registry.example.org/library/demo@sha256:abc",
			pid: 77,
			state: .running
		),
		ociVersion: "1.1.0",
		annotations: ["com.example.state": "ready"]
	)
	try store.save(snapshot)

	let installer = OrlixOCIEnvironmentInstaller(registry: registry)
	let report = try installer.state(arguments: [
		"orlix",
		"state",
		"--id",
		"oci-state-args",
	])

	XCTAssertEqual(report.id, "oci-state-args")
	XCTAssertEqual(report.status, .running)
	XCTAssertEqual(report.pid, 77)
	XCTAssertEqual(
		report.bundle,
		"oci://registry.example.org/library/demo@sha256:abc"
	)
	XCTAssertEqual(report.annotations["com.example.state"], "ready")
}

func testOCIEnvironmentInstallerLifecycleArgumentsDriveRuntimeActions() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-lifecycle-arguments-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let runtime = OrlixOCIRuntime(registry: registry)
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)

	let waitBundleURL = scratch.appendingPathComponent("wait-bundle", isDirectory: true)
	try fileManager.createDirectory(
		at: waitBundleURL.appendingPathComponent("rootfs", isDirectory: true),
		withIntermediateDirectories: true
	)
	try nonRootOCIRuntimeConfig().write(
		to: waitBundleURL.appendingPathComponent("config.json")
	)
	let createdForWait = try runtime.create(
		bundleURL: waitBundleURL,
		id: "oci-lifecycle-wait"
	)
	try Data("base".utf8).write(to: createdForWait.importPlan.storageLayout.baseImageURL)
	try Data("state".utf8).write(to: createdForWait.importPlan.storageLayout.stateImageURL)
	let waitDriver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 66,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 66, exitStatus: 3)
		)
	)

	let started = try installer.start(
		arguments: ["orlix", "start", "oci-lifecycle-wait"],
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: waitDriver,
		fileManager: fileManager
	)
	let completed = try installer.wait(
		arguments: ["wait", "--id=oci-lifecycle-wait"],
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: waitDriver,
		fileManager: fileManager
	)
	let deleted = try installer.delete(
		arguments: ["delete", "--name", "oci-lifecycle-wait"],
		fileManager: fileManager
	)

	XCTAssertEqual(started.stateReport.status, .running)
	XCTAssertEqual(started.stateReport.pid, 66)
	XCTAssertEqual(completed.stateReport.status, .stopped)
	XCTAssertEqual(completed.stateReport.exitStatus, 3)
	XCTAssertEqual(deleted.lifecycleState, .deleted)
	XCTAssertEqual(waitDriver.events, [
		"start:created:nil",
		"wait:running:66",
	])

	let killBundleURL = scratch.appendingPathComponent("kill-bundle", isDirectory: true)
	try fileManager.createDirectory(
		at: killBundleURL.appendingPathComponent("rootfs", isDirectory: true),
		withIntermediateDirectories: true
	)
	try nonRootOCIRuntimeConfig().write(
		to: killBundleURL.appendingPathComponent("config.json")
	)
	let createdForKill = try runtime.create(
		bundleURL: killBundleURL,
		id: "oci-lifecycle-kill"
	)
	try Data("base".utf8).write(to: createdForKill.importPlan.storageLayout.baseImageURL)
	try Data("state".utf8).write(to: createdForKill.importPlan.storageLayout.stateImageURL)
	let killDriver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 67,
		completion: .signaled(
			OrlixOCIRuntimeProcessSignalObservation(pid: 67, signal: 2)
		)
	)

	_ = try installer.start(
		arguments: ["start", "--id", "oci-lifecycle-kill"],
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: killDriver,
		fileManager: fileManager
	)
	let signaled = try installer.kill(
		arguments: ["orlix", "kill", "oci-lifecycle-kill", "--signal", "2"],
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: killDriver,
		fileManager: fileManager
	)

	XCTAssertEqual(signaled.signal, 2)
	XCTAssertEqual(signaled.stateReport.status, .running)
	XCTAssertEqual(signaled.stateReport.pid, 67)
	XCTAssertEqual(killDriver.events, [
		"start:created:nil",
		"signal:running:67:2",
	])
}

func testOCIEnvironmentInstallerHealthcheckRunsImageCommand() throws {
let fileManager = FileManager.default
let scratch = fileManager.temporaryDirectory.appendingPathComponent(
"orlix-oci-healthcheck-run-\(UUID().uuidString)",
isDirectory: true
)
try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
defer { try? fileManager.removeItem(at: scratch) }

let registry = OrlixEnvironmentRegistry(
linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
)
let runtime = OrlixOCIRuntime(registry: registry)
let installer = OrlixOCIEnvironmentInstaller(registry: registry)
let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
try fileManager.createDirectory(
at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
withIntermediateDirectories: true
)
try nonRootOCIRuntimeConfig().write(
to: bundleURL.appendingPathComponent("config.json")
)
let created = try runtime.create(bundleURL: bundleURL, id: "oci-healthcheck-run")
try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
let saved = try registry.load(environmentID: "oci-healthcheck-run")
try registry.save(
descriptor(
saved,
healthcheck: OrlixEnvironmentHealthcheck(
test: ["CMD", "/usr/bin/curl", "-f", "http://127.0.0.1/health"]
)
),
fileManager: fileManager
)
let driver = try RecordingOCIRuntimeProcessObservationDriver(
startPID: 71,
completion: .exited(
OrlixOCIRuntimeProcessExitObservation(pid: 71, exitStatus: 0)
)
)

let result = try installer.healthcheck(
arguments: ["orlix", "healthcheck", "--id", "oci-healthcheck-run"],
terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
using: driver,
fileManager: fileManager
)

XCTAssertEqual(result.id, "oci-healthcheck-run")
XCTAssertEqual(
result.command,
["/usr/bin/curl", "-f", "http://127.0.0.1/health"]
)
XCTAssertEqual(driver.startCommands, [
["/usr/bin/curl", "-f", "http://127.0.0.1/health"]
])
XCTAssertEqual(driver.events, [
"start:created:nil",
"wait:running:71",
])
XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
XCTAssertEqual(result.runResult.completedStateReport.exitStatus, 0)
}

func testOCIEnvironmentInstallerHealthcheckRunsShellCommand() throws {
let fileManager = FileManager.default
let scratch = fileManager.temporaryDirectory.appendingPathComponent(
"orlix-oci-healthcheck-shell-\(UUID().uuidString)",
isDirectory: true
)
try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
defer { try? fileManager.removeItem(at: scratch) }

let registry = OrlixEnvironmentRegistry(
linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
)
let runtime = OrlixOCIRuntime(registry: registry)
let installer = OrlixOCIEnvironmentInstaller(registry: registry)
let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
try fileManager.createDirectory(
at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
withIntermediateDirectories: true
)
try nonRootOCIRuntimeConfig().write(
to: bundleURL.appendingPathComponent("config.json")
)
let created = try runtime.create(bundleURL: bundleURL, id: "oci-healthcheck-shell")
try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
let saved = try registry.load(environmentID: "oci-healthcheck-shell")
try registry.save(
descriptor(
saved,
healthcheck: OrlixEnvironmentHealthcheck(
test: ["CMD-SHELL", "test -f /tmp/healthy"]
)
),
fileManager: fileManager
)
let driver = try RecordingOCIRuntimeProcessObservationDriver(
startPID: 72,
completion: .exited(
OrlixOCIRuntimeProcessExitObservation(pid: 72, exitStatus: 0)
)
)

let result = try installer.healthcheck(
id: "oci-healthcheck-shell",
terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
using: driver,
fileManager: fileManager
)

XCTAssertEqual(result.command, ["/bin/sh", "-c", "test -f /tmp/healthy"])
XCTAssertEqual(driver.startCommands, [
["/bin/sh", "-c", "test -f /tmp/healthy"]
])
}

func testOCIEnvironmentInstallerHealthcheckRejectsUnavailableChecks() throws {
let fileManager = FileManager.default
let scratch = fileManager.temporaryDirectory.appendingPathComponent(
"orlix-oci-healthcheck-invalid-\(UUID().uuidString)",
isDirectory: true
)
try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
defer { try? fileManager.removeItem(at: scratch) }

let registry = OrlixEnvironmentRegistry(
linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
)
let runtime = OrlixOCIRuntime(registry: registry)
let installer = OrlixOCIEnvironmentInstaller(registry: registry)

func createEnvironment(
_ id: String,
healthcheck: OrlixEnvironmentHealthcheck?
) throws {
let bundleURL = scratch.appendingPathComponent(id, isDirectory: true)
try fileManager.createDirectory(
at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
withIntermediateDirectories: true
)
try nonRootOCIRuntimeConfig().write(
to: bundleURL.appendingPathComponent("config.json")
)
let created = try runtime.create(bundleURL: bundleURL, id: id)
try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
let saved = try registry.load(environmentID: id)
try registry.save(
descriptor(saved, healthcheck: healthcheck),
fileManager: fileManager
)
}

try createEnvironment("oci-healthcheck-missing", healthcheck: nil)
try createEnvironment(
"oci-healthcheck-disabled",
healthcheck: OrlixEnvironmentHealthcheck(test: ["NONE"])
)
try createEnvironment(
"oci-healthcheck-invalid",
healthcheck: OrlixEnvironmentHealthcheck(test: ["CMD"])
)

XCTAssertThrowsError(
try installer.healthcheck(id: "oci-healthcheck-missing", fileManager: fileManager)
) { error in
XCTAssertEqual(
error as? OrlixOCIEnvironmentHealthcheckError,
.missingHealthcheck("oci-healthcheck-missing")
)
}
XCTAssertThrowsError(
try installer.healthcheck(id: "oci-healthcheck-disabled", fileManager: fileManager)
) { error in
XCTAssertEqual(
error as? OrlixOCIEnvironmentHealthcheckError,
.disabledHealthcheck("oci-healthcheck-disabled")
)
}
XCTAssertThrowsError(
try installer.healthcheck(id: "oci-healthcheck-invalid", fileManager: fileManager)
) { error in
XCTAssertEqual(
error as? OrlixOCIEnvironmentHealthcheckError,
.invalidHealthcheckTest("oci-healthcheck-invalid", ["CMD"])
)
}
}

func testOCIEnvironmentInstallerRunCarriesDescriptorProcessDefaultsIntoRuntimeConfig()
throws
{
let fileManager = FileManager.default
let scratch = fileManager.temporaryDirectory.appendingPathComponent(
"orlix-oci-registry-process-bridge-\(UUID().uuidString)",
isDirectory: true
)
try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
defer { try? fileManager.removeItem(at: scratch) }

let registry = OrlixEnvironmentRegistry(
linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
)
let id = "oci-registry-process-bridge"
let descriptor = OrlixEnvironmentDescriptor(
id: id,
source: .ociLayout,
platform: "linux/arm64",
rootImageIdentifier: id,
defaultCommand: ["/bin/sh", "-lc", "echo bridge"],
defaultEnvironment: ["PATH": "/usr/bin:/bin"],
defaultWorkingDirectory: "/work",
		defaultUserID: 1000,
		defaultGroupID: 100,
		defaultSupplementaryGroups: [44, 45],
		defaultCapabilities: OrlixEnvironmentCapabilities(
			bounding: ["CAP_CHOWN"],
			permitted: ["CAP_CHOWN"],
			effective: ["CAP_CHOWN"]
		),
		defaultNoNewPrivileges: true,
		defaultCloseAdditionalFds: true,
		defaultTerminal: true,
		defaultTerminalRows: 33,
		defaultTerminalColumns: 120,
		defaultOOMScoreAdjustment: 12,
		defaultScheduler: OrlixEnvironmentScheduler(
			policy: "SCHED_FIFO",
			priority: 1
		),
		defaultIOPriority: OrlixEnvironmentIOPriority(
			class: "IOPRIO_CLASS_BE",
			priority: 4
		),
		defaultCPUAffinity: OrlixEnvironmentCPUAffinity(mask: "0-1"),
		defaultUmask: 0o022,
		defaultRlimits: [
			OrlixEnvironmentRlimit(type: "RLIMIT_NOFILE", soft: 64, hard: 128),
		],
		defaultPersonalityDomain: "LINUX",
		hostname: "orlix-demo",
		domainname: "example.test",
		rootReadonly: true,
		rootPropagation: .shared,
		sysctls: ["kernel.hostname": "orlix-demo"],
		maskedPaths: ["/proc/kcore"],
		readonlyPaths: ["/proc/sys"],
		cgroupsPath: "/orlix/oci/bridge",
		cgroupPidsLimit: 32,
		cgroupCPUMax: OrlixEnvironmentCgroupCPUMax(
			quotaMicros: 50_000,
			periodMicros: 100_000
		),
		cgroupCPUWeight: 100,
		cgroupMemoryMax: 1_048_576,
		cgroupIOWeight: 100,
		cgroupUnified: [
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "io.max",
				value: "8:0 rbps=1024"
			),
		],
		deviceNodes: [
			OrlixEnvironmentDeviceNode(
				path: "/dev/fuse",
				type: "c",
				major: 10,
				minor: 229
			),
		],
		timeOffsets: [
			OrlixEnvironmentTimeOffset(
				clock: "monotonic",
				secs: 1,
				nanosecs: 2
			),
		],
		uidMappings: [
			OrlixEnvironmentIDMapping(containerID: 0, hostID: 1000, size: 1),
		],
		gidMappings: [
			OrlixEnvironmentIDMapping(containerID: 0, hostID: 1000, size: 1),
		],
		namespaces: ["time", "user", "uts"],
		namespacePaths: ["network": "/proc/1/ns/net"],
		tmpfsMounts: [
			try OrlixEnvironmentTmpfsMount(
				targetPath: "/run/oci-cache",
				noSuid: true,
				noDev: true,
				noExec: true,
				data: "size=64m,mode=0755"
			),
		]
	)
try registry.save(descriptor, fileManager: fileManager)
let layout = try registry.layout(forEnvironmentID: id)
try Data("base".utf8).write(to: layout.baseImageURL)
try Data("state".utf8).write(to: layout.stateImageURL)
let lifecycle = try OrlixOCIRuntimeLifecycleController(
config: try OrlixOCIRuntimeConfigParser().parse(minimalOCIRuntimeConfig()),
id: id,
bundlePath: "oci://registry.example.org/library/demo@sha256:bridge"
).create()
try OrlixOCIRuntimeLifecycleStore(registry: registry).save(
lifecycle,
fileManager: fileManager
)
let driver = try RecordingOCIRuntimeProcessObservationDriver(
startPID: 73,
completion: .exited(
OrlixOCIRuntimeProcessExitObservation(pid: 73, exitStatus: 0)
)
)

let result = try OrlixOCIEnvironmentInstaller(registry: registry).run(
id: id,
terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
using: driver,
fileManager: fileManager
)

XCTAssertEqual(result.completedStateReport.exitStatus, 0)
XCTAssertEqual(driver.startCommands, [["/bin/sh", "-lc", "echo bridge"]])
XCTAssertEqual(
driver.startConsoleSizes,
[OrlixOCIRuntimeConsoleSize(height: 33, width: 120)]
)
let commandLine = try XCTUnwrap(driver.startKernelCommandLines.first ?? nil)
XCTAssertTrue(commandLine.contains("orlix.terminal=1"))
XCTAssertTrue(commandLine.contains("orlix.terminal.rows=33"))
XCTAssertTrue(commandLine.contains("orlix.terminal.cols=120"))
XCTAssertTrue(commandLine.contains("orlix.uid=1000"))
XCTAssertTrue(commandLine.contains("orlix.gid=100"))
XCTAssertTrue(commandLine.contains("orlix.suppgid0=44"))
	XCTAssertTrue(commandLine.contains("orlix.suppgid1=45"))
	XCTAssertTrue(commandLine.contains("orlix.cap.bounding=CAP_CHOWN"))
	XCTAssertTrue(commandLine.contains("orlix.cap.permitted=CAP_CHOWN"))
	XCTAssertTrue(commandLine.contains("orlix.cap.effective=CAP_CHOWN"))
	XCTAssertTrue(commandLine.contains("orlix.nonewprivs=1"))
	XCTAssertTrue(commandLine.contains("orlix.closefds=1"))
	XCTAssertTrue(commandLine.contains("orlix.oomscoreadj=12"))
	XCTAssertTrue(commandLine.contains("orlix.scheduler.policy=SCHED_FIFO"))
	XCTAssertTrue(commandLine.contains("orlix.scheduler.priority=1"))
	XCTAssertTrue(commandLine.contains("orlix.ioprio.class=IOPRIO_CLASS_BE"))
	XCTAssertTrue(commandLine.contains("orlix.ioprio.priority=4"))
	XCTAssertTrue(commandLine.contains("orlix.cpuaffinity=0-1"))
	XCTAssertTrue(commandLine.contains("orlix.umask=18"))
	XCTAssertTrue(commandLine.contains("orlix.rlimit0=RLIMIT_NOFILE:64:128"))
	XCTAssertTrue(commandLine.contains("orlix.personality=LINUX"))
	XCTAssertTrue(commandLine.contains("orlix.root.readonly=1"))
	XCTAssertTrue(commandLine.contains("orlix.root.propagation=shared"))
	XCTAssertTrue(commandLine.contains("orlix.sysctl0=kernel.hostname=orlix-demo"))
	XCTAssertTrue(commandLine.contains("orlix.maskedpath0=/proc/kcore"))
	XCTAssertTrue(commandLine.contains("orlix.readonlypath0=/proc/sys"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.path=/orlix/oci/bridge"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.pids.max=32"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.cpu.max=50000:100000"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.cpu.weight=100"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.memory.max=1048576"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.io.weight=100"))
	XCTAssertTrue(commandLine.contains("orlix.cgroups.unified0=io.max:8:0%20rbps=1024"))
	XCTAssertTrue(commandLine.contains("orlix.device.path0=/dev/fuse"))
	XCTAssertTrue(commandLine.contains("orlix.device.type0=c"))
	XCTAssertTrue(commandLine.contains("orlix.device.major0=10"))
	XCTAssertTrue(commandLine.contains("orlix.device.minor0=229"))
	XCTAssertTrue(commandLine.contains("orlix.timeoffset0=monotonic:1:2"))
	XCTAssertTrue(commandLine.contains("orlix.uidmap0=0:1000:1"))
	XCTAssertTrue(commandLine.contains("orlix.gidmap0=0:1000:1"))
	XCTAssertTrue(commandLine.contains("orlix.namespace0=time"))
	XCTAssertTrue(commandLine.contains("orlix.namespace1=user"))
	XCTAssertTrue(commandLine.contains("orlix.namespace2=uts"))
	XCTAssertTrue(commandLine.contains("orlix.namespacepath0=network=/proc/1/ns/net"))
	XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.target=/run/oci-cache"))
	XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.nosuid=1"))
	XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.nodev=1"))
	XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.noexec=1"))
	XCTAssertTrue(commandLine.contains("orlix.mount.tmpfs0.data=size=64m%2Cmode=0755"))
}

func testOCIRuntimeCreateStateAndDeleteUseDurableStore() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-api-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let runtime = OrlixOCIRuntime(registry: registry)

		let created = try runtime.create(
			bundleURL: bundleURL,
			id: "oci-created"
		)
		let report = try runtime.state(id: "oci-created")
		let savedEnvironment = try registry.load(environmentID: "oci-created")
		let preparedMarker = created.importPlan.materializationPlan
			.baseTreeDirectory
			.appendingPathComponent("root-marker")
		let descriptorURL = try registry.descriptorURL(forEnvironmentID: "oci-created")
		let rootDirectory = created.importPlan.storageLayout.rootDirectory
		let importScratchDirectory = created.importPlan.storageLayout.importScratchDirectory

		XCTAssertEqual(created.lifecycle.record.state, .created)
		XCTAssertEqual(created.stateReport.status, .created)
		XCTAssertEqual(report.status, .created)
		XCTAssertNil(report.pid)
		XCTAssertEqual(report.bundle, bundleURL.path)
		XCTAssertEqual(savedEnvironment, created.environment)
		XCTAssertEqual(
			try String(contentsOf: preparedMarker, encoding: .utf8),
			"bundle-root\n"
		)

		let deleted = try runtime.delete(id: "oci-created")
		XCTAssertEqual(deleted.id, "oci-created")
		XCTAssertEqual(deleted.deletedRecord.state, .deleted)
		XCTAssertFalse(fileManager.fileExists(atPath: descriptorURL.path))
		XCTAssertFalse(fileManager.fileExists(atPath: rootDirectory.path))
		XCTAssertFalse(fileManager.fileExists(atPath: importScratchDirectory.path))
		XCTAssertFalse(fileManager.fileExists(atPath: preparedMarker.path))
		XCTAssertThrowsError(try runtime.state(id: "oci-created")) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleStoreError,
				.missingRecord("oci-created")
			)
		}
	}

	func testOCIRuntimeCreateMaterializedRunsMaterializationBeforeCreatedState() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-materialized-create-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let runtime = OrlixOCIRuntime(
			registry: OrlixEnvironmentRegistry(
				linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
				cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
				scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
			)
		)
		let runner = RecordingMaterializationCommandRunner(
			createsPlaceholderImagesForTruncateCommands: true
		)

		let created = try runtime.createMaterialized(
			bundleURL: bundleURL,
			id: "oci-materialized-create",
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs",
			runner: runner
		)

		XCTAssertEqual(created.stateReport.status, .created)
		XCTAssertEqual(
			created.materializationResult.commands,
			try created.importPlan.materializationCommands(
				mke2fsExecutable: "orlix-mke2fs",
				truncateExecutable: "orlix-truncate",
				debugfsExecutable: "orlix-debugfs"
			)
		)
		XCTAssertEqual(runner.commands, created.materializationResult.commands)
		XCTAssertTrue(
			fileManager.fileExists(atPath: created.importPlan.storageLayout.baseImageURL.path)
		)
		XCTAssertTrue(
			fileManager.fileExists(atPath: created.importPlan.storageLayout.stateImageURL.path)
		)
		XCTAssertEqual(try runtime.state(id: "oci-materialized-create").status, .created)

		let baseImageURL = created.importPlan.storageLayout.baseImageURL
		let stateImageURL = created.importPlan.storageLayout.stateImageURL
		let rootDirectory = created.importPlan.storageLayout.rootDirectory
		let importScratchDirectory = created.importPlan.storageLayout.importScratchDirectory
		_ = try runtime.delete(id: "oci-materialized-create")

		XCTAssertFalse(fileManager.fileExists(atPath: baseImageURL.path))
		XCTAssertFalse(fileManager.fileExists(atPath: stateImageURL.path))
	XCTAssertFalse(fileManager.fileExists(atPath: rootDirectory.path))
	XCTAssertFalse(fileManager.fileExists(atPath: importScratchDirectory.path))
}

func testOCIEnvironmentInstallerListsPreparedEnvironmentStates() throws {
    let fileManager = FileManager.default
    let scratch = fileManager.temporaryDirectory.appendingPathComponent(
        "orlix-oci-list-prepared-\(UUID().uuidString)",
        isDirectory: true
    )
    try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
    defer { try? fileManager.removeItem(at: scratch) }

    let registry = OrlixEnvironmentRegistry(
        linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
        cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
        scratchRoot: scratch.appendingPathComponent("scratch", isDirectory: true)
    )
    let lifecycleStore = OrlixOCIRuntimeLifecycleStore(registry: registry)
    let config = try OrlixOCIRuntimeConfigParser()
        .parse(nonRootOCIRuntimeConfig())
    let installer = OrlixOCIEnvironmentInstaller(registry: registry)

    let alphaDescriptor = OrlixEnvironmentDescriptor(
        id: "oci-alpha",
        source: .ociLayout,
        platform: "linux/arm64",
        rootImageIdentifier: "orlix.env.oci-alpha",
        defaultCommand: ["/bin/sh"],
        defaultEnvironment: ["PATH": "/usr/bin:/bin"],
        defaultWorkingDirectory: "/",
        defaultUserID: 0,
        defaultGroupID: 0
    )
    let betaDescriptor = OrlixEnvironmentDescriptor(
        id: "oci-beta",
        source: .ociLayout,
        platform: "linux/arm64",
        rootImageIdentifier: "orlix.env.oci-beta",
        defaultCommand: ["/usr/bin/env"],
        defaultEnvironment: ["PATH": "/usr/bin:/bin"],
        defaultWorkingDirectory: "/",
        defaultUserID: 0,
        defaultGroupID: 0
    )
    let nonOCIEnvironment = OrlixEnvironmentDescriptor(
        id: "copied-root",
        source: .copiedEnvironment(parentID: "default"),
        platform: "linux/arm64",
        rootImageIdentifier: "orlix.env.copied-root",
        defaultCommand: ["/bin/sh"],
        defaultEnvironment: ["PATH": "/usr/bin:/bin"],
        defaultWorkingDirectory: "/",
        defaultUserID: 0,
        defaultGroupID: 0
    )

    try registry.save(betaDescriptor)
    try registry.save(nonOCIEnvironment)
    try registry.save(alphaDescriptor)
    try lifecycleStore.save(
        OrlixOCIRuntimeLifecycleController(
            config: config,
            id: "oci-alpha",
            bundlePath: "/bundles/oci-alpha"
        ).create()
    )
    try lifecycleStore.save(
        OrlixOCIRuntimeLifecycleController(
            config: config,
            id: "oci-beta",
            bundlePath: "/bundles/oci-beta"
        )
        .create()
        .start(pid: 42)
    )

    let prepared = try installer.listPreparedEnvironments()

    XCTAssertEqual(prepared.map(\.id), ["oci-alpha", "oci-beta"])
    XCTAssertEqual(prepared[0].platform, "linux/arm64")
    XCTAssertEqual(prepared[0].defaultCommand, ["/bin/sh"])
    XCTAssertEqual(prepared[0].lifecycleState, .created)
    XCTAssertEqual(prepared[0].stateReport?.status, .created)
    XCTAssertNil(prepared[0].stateReport?.pid)
	XCTAssertEqual(prepared[1].defaultCommand, ["/usr/bin/env"])
	XCTAssertEqual(prepared[1].lifecycleState, .running)
	XCTAssertEqual(prepared[1].stateReport?.status, .running)
	XCTAssertEqual(prepared[1].stateReport?.pid, 42)

	let running = try installer.list(arguments: [
		"orlix",
		"ps",
		"--state",
		"running",
	])
	XCTAssertEqual(running.map(\.id), ["oci-beta"])

	let created = try installer.listPreparedEnvironments(arguments: [
		"list",
		"--status=created",
	])
	XCTAssertEqual(created.map(\.id), ["oci-alpha"])
}

func testOCIEnvironmentInstallerMaterializesBundleAndBuildsSession() throws {
    let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-installer-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
	defer { try? fileManager.removeItem(at: scratch) }
	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try "bundle-root\n".write(
		to: rootfsURL.appendingPathComponent("root-marker"),
		atomically: true,
		encoding: .utf8
	)
	try Data("""
	{
		"ociVersion" : "1.1.0",
		"root" : { "path" : "rootfs" },
		"process" : {
			"terminal" : false,
			"args" : ["/bin/true"],
			"cwd" : "/"
		}
	}
	""".utf8).write(to: bundleURL.appendingPathComponent("config.json"))
	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let installer = OrlixOCIEnvironmentInstaller(registry: registry)
	let tools = OrlixOCIEnvironmentMaterializationTools(
		mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
		truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
		debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
	)
	let recorder = RecordingPublicOCIInstallerCommandRunner()
	let installed = try installer.install(
		bundleURL: bundleURL,
		id: "oci-installed-session",
		tools: tools,
		fileManager: fileManager
	) { executable, arguments in
		try recorder.run(executable: executable, arguments: arguments)
	}
	let layout = try registry.layout(forEnvironmentID: "oci-installed-session")
	XCTAssertEqual(installed.id, "oci-installed-session")
	XCTAssertEqual(installed.bundleURL, bundleURL)
	XCTAssertEqual(installed.stateReport.status, .created)
	XCTAssertEqual(recorder.executables.first, tools.truncate)
	XCTAssertEqual(recorder.executables.filter { $0 == tools.mke2fs }.count, 2)
	XCTAssertEqual(recorder.executables.filter { $0 == tools.debugfs }.count, 2)
	XCTAssertTrue(fileManager.fileExists(atPath: layout.baseImageURL.path))
	XCTAssertTrue(fileManager.fileExists(atPath: layout.stateImageURL.path))
	XCTAssertEqual(
		try registry.load(environmentID: "oci-installed-session").defaultCommand,
		["/bin/true"]
	)
	let session = try installer.session(
		bundleURL: bundleURL,
		id: "oci-installed-session",
		terminal: OrlixTerminalSession()
	)
	let rootImage = try XCTUnwrap(session.materializedRootImageForTesting)
	XCTAssertEqual(rootImage.baseImageURL, layout.baseImageURL)
	XCTAssertEqual(rootImage.stateImageURL, layout.stateImageURL)
		let commandLine = try XCTUnwrap(session.bootConfig.kernelCommandLine)
		XCTAssertTrue(commandLine.contains("orlix.exec=/bin/true"))
		XCTAssertTrue(commandLine.hasPrefix("orlix.terminal=0 "))

		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 109,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 109, exitStatus: 0)
			)
		)
		let run = try installer.run(
			id: "oci-installed-session",
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			using: driver
		)

		XCTAssertEqual(run.id, "oci-installed-session")
		XCTAssertEqual(run.startedStateReport.status, .running)
		XCTAssertEqual(run.completedStateReport.status, .stopped)
		XCTAssertEqual(run.completedStateReport.exitStatus, 0)
		XCTAssertEqual(
			try installer.state(id: "oci-installed-session"),
			run.completedStateReport
		)
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:109",
			]
		)

		let deleted = try installer.delete(id: "oci-installed-session")
		XCTAssertEqual(deleted.id, "oci-installed-session")
		XCTAssertEqual(deleted.lifecycleState, .deleted)
		XCTAssertThrowsError(try installer.state(id: "oci-installed-session")) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleStoreError,
				.missingRecord("oci-installed-session")
			)
		}
		XCTAssertFalse(fileManager.fileExists(atPath: layout.rootDirectory.path))
	}

	func testOCIEnvironmentInstallerRunsBundleByInstallingThenStarting() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-installer-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(at: scratch, withIntermediateDirectories: true)
		defer { try? fileManager.removeItem(at: scratch) }
		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try Data("""
		{
			"ociVersion" : "1.1.0",
			"root" : { "path" : "rootfs" },
			"process" : {
				"terminal" : false,
				"args" : ["/bin/true"],
				"cwd" : "/"
			}
		}
		""".utf8).write(to: bundleURL.appendingPathComponent("config.json"))
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let tools = OrlixOCIEnvironmentMaterializationTools(
			mke2fs: URL(fileURLWithPath: "/usr/local/bin/orlix-mke2fs"),
			truncate: URL(fileURLWithPath: "/usr/local/bin/orlix-truncate"),
			debugfs: URL(fileURLWithPath: "/usr/local/bin/orlix-debugfs")
		)
		let recorder = RecordingPublicOCIInstallerCommandRunner()
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 110,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 110, exitStatus: 0)
			)
		)

		let result = try installer.run(
			bundleURL: bundleURL,
			id: "oci-installed-run",
			tools: tools,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			using: driver,
			fileManager: fileManager
		) { executable, arguments in
			try recorder.run(executable: executable, arguments: arguments)
		}
		let layout = try registry.layout(forEnvironmentID: "oci-installed-run")

		XCTAssertEqual(result.installResult.id, "oci-installed-run")
		XCTAssertEqual(result.installResult.stateReport.status, .created)
		XCTAssertEqual(result.runResult.id, "oci-installed-run")
		XCTAssertEqual(result.runResult.startedStateReport.status, .running)
		XCTAssertEqual(result.runResult.completedStateReport.status, .stopped)
		XCTAssertEqual(result.runResult.completedStateReport.exitStatus, 0)
		XCTAssertEqual(
			try installer.state(id: "oci-installed-run"),
			result.runResult.completedStateReport
		)
		XCTAssertEqual(recorder.executables.first, tools.truncate)
		XCTAssertEqual(recorder.executables.filter { $0 == tools.mke2fs }.count, 2)
		XCTAssertEqual(recorder.executables.filter { $0 == tools.debugfs }.count, 2)
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:110",
			]
		)
		XCTAssertTrue(fileManager.fileExists(atPath: layout.rootDirectory.path))

		let deleted = try installer.delete(id: "oci-installed-run")
		XCTAssertEqual(deleted.lifecycleState, .deleted)
		XCTAssertFalse(fileManager.fileExists(atPath: layout.rootDirectory.path))
	}

	func testOCIRuntimeRunUsesMaterializedCreateRootImages() throws {
		let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-materialized-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let runtime = OrlixOCIRuntime(
			registry: OrlixEnvironmentRegistry(
				linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
				cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
				scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
			)
		)
		let materializationRunner = RecordingMaterializationCommandRunner(
			createsPlaceholderImagesForTruncateCommands: true
		)
		_ = try runtime.createMaterialized(
			bundleURL: bundleURL,
			id: "oci-materialized-run",
			runner: materializationRunner
		)
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 103,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 103, exitStatus: 0)
			)
		)

		let result = try runtime.run(
			id: "oci-materialized-run",
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			using: driver
		)

		XCTAssertEqual(result.startedEnvironment.stateReport.status, .running)
		XCTAssertEqual(result.completedEnvironment.stateReport.status, .stopped)
		XCTAssertEqual(result.completedEnvironment.stateReport.exitStatus, 0)
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:103",
			]
		)
	}

	func testOCIRuntimeRunBundleMaterializesCreatesStartsAndWaits() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-bundle-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try "bundle-root\n".write(
			to: rootfsURL.appendingPathComponent("root-marker"),
			atomically: true,
			encoding: .utf8
		)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let runtime = OrlixOCIRuntime(
			registry: OrlixEnvironmentRegistry(
				linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
				cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
				scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
			)
		)
		let materializationRunner = RecordingMaterializationCommandRunner(
			createsPlaceholderImagesForTruncateCommands: true
		)
		let processDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 104,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 104, exitStatus: 9)
			)
		)

		let result = try runtime.run(
			bundleURL: bundleURL,
			id: "oci-bundle-run",
			mke2fsExecutable: "orlix-mke2fs",
			truncateExecutable: "orlix-truncate",
			debugfsExecutable: "orlix-debugfs",
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			materializationRunner: materializationRunner,
			processDriver: processDriver
		)

		XCTAssertEqual(result.createdEnvironment.stateReport.status, .created)
		XCTAssertEqual(result.startedEnvironment.stateReport.status, .running)
		XCTAssertEqual(result.completedEnvironment.stateReport.status, .stopped)
		XCTAssertEqual(result.completedEnvironment.stateReport.exitStatus, 9)
		XCTAssertEqual(try runtime.state(id: "oci-bundle-run"), result.completedEnvironment.stateReport)
		XCTAssertEqual(
			materializationRunner.commands,
			result.createdEnvironment.materializationResult.commands
		)
		XCTAssertEqual(
			processDriver.events,
			[
				"start:created:nil",
				"wait:running:104",
			]
		)
	}

	func testOCIRuntimeRunBundleRejectsExistingLifecycleBeforeMaterialization() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-bundle-run-duplicate-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let runtime = OrlixOCIRuntime(
			registry: OrlixEnvironmentRegistry(
				linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
				cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
				scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
			)
		)
		_ = try runtime.create(bundleURL: bundleURL, id: "oci-bundle-run-duplicate")
		let materializationRunner = RecordingMaterializationCommandRunner(
			createsPlaceholderImagesForTruncateCommands: true
		)
		let processDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 105,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 105, exitStatus: 0)
			)
		)

		XCTAssertThrowsError(
			try runtime.run(
				bundleURL: bundleURL,
				id: "oci-bundle-run-duplicate",
				materializationRunner: materializationRunner,
				processDriver: processDriver
			)
		) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeError,
				.environmentAlreadyExists("oci-bundle-run-duplicate")
			)
		}
		XCTAssertEqual(materializationRunner.commands, [])
		XCTAssertEqual(processDriver.events, [])
		XCTAssertEqual(try runtime.state(id: "oci-bundle-run-duplicate").status, .created)
	}

	func testOCIRuntimeRunEphemeralDeletesResourcesAfterCompletion() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-ephemeral-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)
		let runtime = OrlixOCIRuntime(
			registry: OrlixEnvironmentRegistry(
				linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
				cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
				scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
			)
		)
		let materializationRunner = RecordingMaterializationCommandRunner(
			createsPlaceholderImagesForTruncateCommands: true
		)
		let processDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 106,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 106, exitStatus: 11)
			)
		)

		let result = try runtime.runEphemeral(
			bundleURL: bundleURL,
			id: "oci-ephemeral-run",
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			materializationRunner: materializationRunner,
			processDriver: processDriver
		)
		let storageLayout = result.createdEnvironment.importPlan.storageLayout

		XCTAssertEqual(result.startedEnvironment.stateReport.status, .running)
		XCTAssertEqual(result.completedEnvironment.stateReport.status, .stopped)
		XCTAssertEqual(result.completedEnvironment.stateReport.exitStatus, 11)
		XCTAssertEqual(result.deletedEnvironment.id, "oci-ephemeral-run")
		XCTAssertEqual(result.deletedEnvironment.deletedRecord.state, .deleted)
		XCTAssertEqual(
			materializationRunner.commands,
			result.createdEnvironment.materializationResult.commands
		)
		XCTAssertEqual(
			processDriver.events,
			[
				"start:created:nil",
				"wait:running:106",
			]
		)
		XCTAssertEqual(
			processDriver.startRootImageIdentifiers,
			[result.createdEnvironment.environment.rootImageIdentifier]
		)
		XCTAssertEqual(
			processDriver.waitRootImageIdentifiers,
			[result.createdEnvironment.environment.rootImageIdentifier]
		)
		XCTAssertThrowsError(try runtime.state(id: "oci-ephemeral-run")) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleStoreError,
				.missingRecord("oci-ephemeral-run")
			)
		}
	XCTAssertFalse(fileManager.fileExists(atPath: storageLayout.rootDirectory.path))
	XCTAssertFalse(fileManager.fileExists(atPath: storageLayout.importScratchDirectory.path))
}

func testOCIRuntimeRunEphemeralCleansUpAfterStartFailure() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-ephemeral-start-failure-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)
	let runtime = OrlixOCIRuntime(
		registry: OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
	)
	let storageLayout = try runtime.registry.layout(
		forEnvironmentID: "oci-ephemeral-start-fails"
	)
	let materializationRunner = RecordingMaterializationCommandRunner(
		createsPlaceholderImagesForTruncateCommands: true
	)
	let processDriver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 107,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 107, exitStatus: 0)
		),
		failsOnStart: true
	)

	XCTAssertThrowsError(
		try runtime.runEphemeral(
			bundleURL: bundleURL,
			id: "oci-ephemeral-start-fails",
			materializationRunner: materializationRunner,
			processDriver: processDriver
		)
	) { error in
		let failure = error as? OrlixOCIRuntimeEphemeralRunFailure
		XCTAssertEqual(failure?.id, "oci-ephemeral-start-fails")
		XCTAssertEqual(
			failure?.originalError as? RecordingOCIRuntimeProcessObservationDriverError,
			.requestedStartFailure
		)
		XCTAssertNil(failure?.cleanupError)
		XCTAssertEqual(failure?.deletedEnvironment?.id, "oci-ephemeral-start-fails")
		XCTAssertEqual(failure?.deletedEnvironment?.deletedRecord.state, .created)
	}

	XCTAssertFalse(materializationRunner.commands.isEmpty)
	XCTAssertEqual(processDriver.events, ["start:created:nil"])
	XCTAssertThrowsError(try runtime.state(id: "oci-ephemeral-start-fails")) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeLifecycleStoreError,
			.missingRecord("oci-ephemeral-start-fails")
		)
	}
	XCTAssertFalse(fileManager.fileExists(atPath: storageLayout.rootDirectory.path))
	XCTAssertFalse(fileManager.fileExists(atPath: storageLayout.importScratchDirectory.path))
}

func testOCIRuntimeDeleteRejectsRunningLifecycleRecord() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-runtime-running-delete-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
		let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
		try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
		try nonRootOCIRuntimeConfig().write(
			to: bundleURL.appendingPathComponent("config.json")
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
		let runtime = OrlixOCIRuntime(registry: registry)
		let created = try runtime.create(
			bundleURL: bundleURL,
			id: "oci-running"
		)
		let rootDirectory = created.importPlan.storageLayout.rootDirectory
		let importScratchDirectory = created.importPlan.storageLayout.importScratchDirectory
		try runtime.lifecycleStore.save(
			try created.lifecycle.start(pid: 123),
			fileManager: fileManager
		)

		XCTAssertThrowsError(try runtime.delete(id: "oci-running")) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .running, action: .delete)
			)
		}
		XCTAssertEqual(try runtime.state(id: "oci-running").status, .running)
		XCTAssertEqual(try runtime.state(id: "oci-running").pid, 123)
		XCTAssertTrue(fileManager.fileExists(atPath: rootDirectory.path))
		XCTAssertTrue(fileManager.fileExists(atPath: importScratchDirectory.path))
	}

func testOCIRuntimeCreateRejectsExistingLifecycleRecord() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-duplicate-create-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	let runtime = OrlixOCIRuntime(
		registry: OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
	)
	_ = try runtime.create(bundleURL: bundleURL, id: "oci-duplicate")

	XCTAssertThrowsError(
		try runtime.create(bundleURL: bundleURL, id: "oci-duplicate")
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeError,
			.environmentAlreadyExists("oci-duplicate")
		)
	}
	XCTAssertEqual(try runtime.state(id: "oci-duplicate").status, .created)
}

func testOCIRuntimeRunStartsAndWaitsCreatedLifecycleRecord() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-run-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	let runtime = OrlixOCIRuntime(
		registry: OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
	)
	let created = try runtime.create(bundleURL: bundleURL, id: "oci-run")
	try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
	try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
	let driver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 101,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 101, exitStatus: 7)
		)
	)

	let result = try runtime.run(
		id: "oci-run",
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver
	)

	XCTAssertEqual(result.startedEnvironment.stateReport.status, .running)
	XCTAssertEqual(result.startedEnvironment.stateReport.pid, 101)
	XCTAssertEqual(result.completedEnvironment.stateReport.status, .stopped)
	XCTAssertEqual(result.completedEnvironment.stateReport.exitStatus, 7)
	XCTAssertEqual(try runtime.state(id: "oci-run"), result.completedEnvironment.stateReport)
	XCTAssertEqual(
		driver.events,
		[
			"start:created:nil",
			"wait:running:101",
		]
	)
}

func testOCIRuntimeRunRejectsUnmaterializedRootBeforeDriverSideEffects() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-run-unmaterialized-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	let runtime = OrlixOCIRuntime(
		registry: OrlixEnvironmentRegistry(
			linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
		)
	)
	let created = try runtime.create(bundleURL: bundleURL, id: "oci-unmaterialized")
	let driver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 102,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 102, exitStatus: 0)
		)
	)

	XCTAssertThrowsError(
		try runtime.run(
			id: "oci-unmaterialized",
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
			using: driver
		)
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeError,
			.missingMaterializedRootImage(
				created.importPlan.storageLayout.baseImageURL.path
			)
		)
	}
	XCTAssertEqual(driver.events, [])
	XCTAssertEqual(try runtime.state(id: "oci-unmaterialized").status, .created)
}

func testOCIRuntimeStartAndWaitResumePersistedLifecycle() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-start-wait-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let runtime = OrlixOCIRuntime(registry: registry)
	let created = try runtime.create(
		bundleURL: bundleURL,
		id: "oci-start-wait"
	)
	try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
	try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
	let driver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 77,
		completion: .exited(
			OrlixOCIRuntimeProcessExitObservation(pid: 77, exitStatus: 5)
		)
	)

	let started = try runtime.start(
		id: "oci-start-wait",
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver
	)
	let completed = try runtime.wait(
		id: "oci-start-wait",
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver
	)

	XCTAssertEqual(started.stateReport.status, .running)
	XCTAssertEqual(started.stateReport.pid, 77)
	XCTAssertNotNil(started.processSession.linuxSession.materializedRootImageForTesting)
	XCTAssertEqual(completed.stateReport.status, .stopped)
	XCTAssertEqual(completed.stateReport.pid, 77)
	XCTAssertEqual(completed.stateReport.exitStatus, 5)
	XCTAssertEqual(try runtime.state(id: "oci-start-wait"), completed.stateReport)
	XCTAssertEqual(
		driver.events,
		[
			"start:created:nil",
			"wait:running:77",
		]
	)
}

func testOCIRuntimeKillResumesRunningLifecycleAndPersistsSignalRequest() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-runtime-kill-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	defer { try? fileManager.removeItem(at: scratch) }

	let bundleURL = scratch.appendingPathComponent("bundle", isDirectory: true)
	let rootfsURL = bundleURL.appendingPathComponent("rootfs", isDirectory: true)
	try fileManager.createDirectory(at: rootfsURL, withIntermediateDirectories: true)
	try nonRootOCIRuntimeConfig().write(
		to: bundleURL.appendingPathComponent("config.json")
	)

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: scratch.appendingPathComponent("state", isDirectory: true),
		cacheRoot: scratch.appendingPathComponent("cache", isDirectory: true),
		scratchRoot: scratch.appendingPathComponent("runtime-scratch", isDirectory: true)
	)
	let runtime = OrlixOCIRuntime(registry: registry)
	let created = try runtime.create(
		bundleURL: bundleURL,
		id: "oci-kill"
	)
	try Data("base".utf8).write(to: created.importPlan.storageLayout.baseImageURL)
	try Data("state".utf8).write(to: created.importPlan.storageLayout.stateImageURL)
	let driver = try RecordingOCIRuntimeProcessObservationDriver(
		startPID: 88,
		completion: .signaled(
			OrlixOCIRuntimeProcessSignalObservation(pid: 88, signal: 15)
		)
	)

	_ = try runtime.start(
		id: "oci-kill",
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver
	)
	let signaled = try runtime.kill(
		id: "oci-kill",
		signal: 15,
		terminal: OrlixTerminalSession(transport: RecordingTerminalTransport()),
		using: driver
	)

	XCTAssertEqual(signaled.signal, 15)
	XCTAssertEqual(signaled.stateReport.status, .running)
	XCTAssertEqual(signaled.stateReport.pid, 88)
	XCTAssertEqual(try runtime.state(id: "oci-kill"), signaled.stateReport)
	XCTAssertEqual(
		driver.events,
		[
			"start:created:nil",
			"signal:running:88:15",
		]
	)
}

func testOCIRuntimeProcessSessionPreservesLinuxSessionAcrossLifecycleUpdates() throws {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-process-session-\(UUID().uuidString)",
		isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: cacheRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: scratchRoot,
			withIntermediateDirectories: true
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		)
		let runningSession = try processSession.start(
			observedProcess: OrlixOCIRuntimeProcessStartObservation(pid: 42)
		)
		let signaledSession = try runningSession.kill(signal: 15)

		XCTAssertTrue(runningSession.linuxSession === processSession.linuxSession)
		XCTAssertTrue(signaledSession.linuxSession === processSession.linuxSession)
		XCTAssertEqual(signaledSession.processHandle.lifecycle.record.state, .running)
		XCTAssertEqual(signaledSession.processHandle.sessionDescriptor.lifecycleState, .running)

		let completedProcess = try signaledSession.exit(
			observedSignal: OrlixOCIRuntimeProcessSignalObservation(
				pid: 42,
				signal: 15
			)
		)
		let report = try completedProcess.stateReport()

		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 143)
	}

	func testOCIRuntimeProcessSessionUsesObservationDriverForStartSignalAndWait() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-driver-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: cacheRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: scratchRoot,
			withIntermediateDirectories: true
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		)
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(
					pid: 42,
					exitStatus: 0
				)
			)
		)

		let runningSession = try processSession.start(using: driver)
		let signaledSession = try runningSession.kill(signal: 15, using: driver)
		let completedProcess = try signaledSession.wait(using: driver)
		let report = try completedProcess.stateReport()

		XCTAssertTrue(runningSession.linuxSession === processSession.linuxSession)
		XCTAssertTrue(signaledSession.linuxSession === processSession.linuxSession)
		XCTAssertEqual(signaledSession.processHandle.lifecycle.record.state, .running)
		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 0)
	XCTAssertEqual(
		driver.events,
		[
			"start:created:nil",
			"signal:running:42:15",
			"wait:running:42"
		]
	)
}

func testOCIRuntimeLinuxSessionObservationDriverRunsFromInitOutput() throws {
	let fixture = try makeCreatedOCIRuntimeProcessSessionFixture(
		scratchName: "orlix-oci-linux-session-driver"
	)
	defer { try? FileManager.default.removeItem(at: fixture.scratch) }

	let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(
		timeout: 1
	) { _ in
		fixture.terminal.emit(
			Data("orlix-init: process started pid=42\n".utf8)
		)
		fixture.terminal.emit(
			Data("orlix-init: process exited pid=42 status=7\n".utf8)
		)
		return .ok
	}

	let result = try fixture.session.runObserved(using: driver)

	XCTAssertEqual(result.startObservation.pid, 42)
	XCTAssertEqual(result.runningSession.processHandle.lifecycle.record.pid, 42)
	XCTAssertEqual(result.completedProcess.lifecycle.record.state, .stopped)
	XCTAssertEqual(result.completedProcess.lifecycle.record.exitStatus, 7)
	XCTAssertEqual(
		result.completionObservation,
		.exited(
			try OrlixOCIRuntimeProcessExitObservation(
				pid: 42,
				exitStatus: 7
			)
		)
	)
}

func testOCIRuntimeLinuxSessionObservationDriverRejectsUnsupportedSignal() throws {
    let fixture = try makeCreatedOCIRuntimeProcessSessionFixture(
        scratchName: "orlix-oci-linux-session-driver-signal"
    )
	defer { try? FileManager.default.removeItem(at: fixture.scratch) }
	let runningSession = try fixture.session.start(observedPID: 42)
	let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(timeout: 1)

	XCTAssertThrowsError(
		try runningSession.kill(signal: 15, using: driver)
	) { error in
		XCTAssertEqual(
			error as? OrlixOCIRuntimeLinuxSessionObservationError,
			.signalUnsupported
        )
    }
}

func testOCIRuntimeLinuxSessionObservationDriverSendsTerminalInterruptSignal() throws {
    let fixture = try makeCreatedOCIRuntimeProcessSessionFixture(
        scratchName: "orlix-oci-linux-session-driver-terminal-signal"
    )
    defer { try? FileManager.default.removeItem(at: fixture.scratch) }
    let runningSession = try fixture.session.start(observedPID: 42)
    let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(timeout: 1)

    let signaledSession = try runningSession.kill(signal: 2, using: driver)

    XCTAssertEqual(fixture.terminal.sentInput, [Data([0x03])])
    XCTAssertEqual(signaledSession.processHandle.lifecycle.record.state, .running)
    XCTAssertEqual(signaledSession.processHandle.lifecycle.record.pid, 42)
}

func testOCIRuntimeProcessSessionValidatesLifecycleBeforeDriverSideEffects() throws {
    let fixture = try makeCreatedOCIRuntimeProcessSessionFixture(
        scratchName: "orlix-oci-process-driver-validation"
    )
		defer { try? FileManager.default.removeItem(at: fixture.scratch) }

		let createdSession = fixture.session
		let waitDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
			)
		)
		XCTAssertThrowsError(try createdSession.wait(using: waitDriver)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .created, action: .exit)
			)
		}
		XCTAssertEqual(waitDriver.events, [])

		let signalDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
			)
		)
		XCTAssertThrowsError(try createdSession.kill(signal: 15, using: signalDriver)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .created, action: .kill)
			)
		}
		XCTAssertEqual(signalDriver.events, [])

		let runningSession = try createdSession.start(observedPID: 42)
		let startDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 43,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
			)
		)
		XCTAssertThrowsError(try runningSession.start(using: startDriver)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .running, action: .start)
			)
		}
		XCTAssertEqual(startDriver.events, [])

		let runDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 44,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
			)
		)
		XCTAssertThrowsError(try runningSession.runObserved(using: runDriver)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidTransition(from: .running, action: .start)
			)
		}
		XCTAssertEqual(runDriver.events, [])

		let invalidSignalDriver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 0)
			)
		)
		XCTAssertThrowsError(try runningSession.kill(signal: 0, using: invalidSignalDriver)) { error in
			XCTAssertEqual(error as? OrlixOCIRuntimeLifecycleError, .invalidSignal(0))
		}
		XCTAssertEqual(invalidSignalDriver.events, [])
	}

	func testOCIRuntimeProcessSessionRunsThroughObservationDriver() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-run-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: cacheRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: scratchRoot,
			withIntermediateDirectories: true
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		)
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(
					pid: 42,
					exitStatus: 0
				)
			)
		)

		let completedProcess = try processSession.run(using: driver)
		let report = try completedProcess.stateReport()

		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 0)
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:42"
			]
		)
	}

	func testOCIRuntimeProcessSessionRunObservedPreservesStartAndCompletionEvidence() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-run-observed-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }
		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(
			at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: cacheRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: scratchRoot,
			withIntermediateDirectories: true
		)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: try configured.create(),
			rootMount: OrlixEnvironmentRootMount.defaultOverlay
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try Data("base".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
		)
		try Data("state".utf8).write(
			to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
		)
		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		)
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				OrlixOCIRuntimeProcessExitObservation(
					pid: 42,
					exitStatus: 0
				)
			)
		)

		let result = try processSession.runObserved(using: driver)
		let report = try result.completedProcess.stateReport()

		XCTAssertEqual(result.startObservation.pid, 42)
		XCTAssertEqual(result.runningSession.processHandle.lifecycle.record.state, .running)
		XCTAssertEqual(result.runningSession.processHandle.lifecycle.record.pid, 42)
		XCTAssertEqual(
			result.completionObservation,
			.exited(
				try OrlixOCIRuntimeProcessExitObservation(
					pid: 42,
					exitStatus: 0
				)
			)
		)
		XCTAssertEqual(report.status, OrlixOCIRuntimeStateStatus.stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 0)
	XCTAssertEqual(
		driver.events,
		[
			"start:created:nil",
			"wait:running:42"
		]
	)
}

	func testOCIRuntimeProcessSessionUsesPersistedRootImageIdentifier() throws {
		let fileManager = FileManager.default
		let scratch = fileManager.temporaryDirectory.appendingPathComponent(
			"orlix-oci-process-root-id-\(UUID().uuidString)",
			isDirectory: true
		)
		try fileManager.createDirectory(
			at: scratch,
			withIntermediateDirectories: true
		)
		defer { try? fileManager.removeItem(at: scratch) }

		let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
		let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
		let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
		try fileManager.createDirectory(at: cacheRoot, withIntermediateDirectories: true)
		try fileManager.createDirectory(at: scratchRoot, withIntermediateDirectories: true)

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: stateRoot,
			cacheRoot: cacheRoot,
			scratchRoot: scratchRoot
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let persistedEnvironment = try config.environmentDescriptor(
			id: "oci-demo",
			rootMount: .defaultOverlay,
			rootImageIdentifier: "orlix.test.environment.oci-demo-root"
		)
		try registry.save(persistedEnvironment)

		let environmentRoot = stateRoot.appendingPathComponent(
			"environments/oci-demo",
			isDirectory: true
		)
		try Data("base".utf8).write(
			to: environmentRoot.appendingPathComponent("base.ext4")
		)
		try Data("state".utf8).write(
			to: environmentRoot.appendingPathComponent("state.ext4")
		)

		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let processSession = try OrlixOCIRuntimeProcessSession(
			lifecycle: try configured.create(),
			rootMount: .defaultOverlay,
			registry: registry,
			terminal: OrlixTerminalSession(transport: RecordingTerminalTransport())
		)
		XCTAssertEqual(
			processSession.processHandle.sessionDescriptor.environment.rootImageIdentifier,
			"orlix.test.environment.oci-demo-root"
		)

		let runningSession = try processSession.start(observedPID: 42)
		XCTAssertEqual(
			runningSession.processHandle.sessionDescriptor.environment.rootImageIdentifier,
			"orlix.test.environment.oci-demo-root"
		)
	}

func testOCIRuntimeProcessSessionPersistsLifecycleTransitionsWhenStoreAttached() throws {
	let fixture = try makeCreatedOCIRuntimeProcessSessionFixture(
		scratchName: "orlix-oci-process-store"
	)
		defer { try? FileManager.default.removeItem(at: fixture.scratch) }
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixture.scratch.appendingPathComponent("state", isDirectory: true),
			cacheRoot: fixture.scratch.appendingPathComponent("cache", isDirectory: true),
			scratchRoot: fixture.scratch.appendingPathComponent("scratch", isDirectory: true)
		)
		let store = OrlixOCIRuntimeLifecycleStore(registry: registry)
		let processSession = OrlixOCIRuntimeProcessSession(
			processHandle: fixture.session.processHandle,
			linuxSession: fixture.session.linuxSession,
			lifecycleStore: store
		)
		let driver = try RecordingOCIRuntimeProcessObservationDriver(
			startPID: 42,
			completion: .exited(
				try OrlixOCIRuntimeProcessExitObservation(
					pid: 42,
					exitStatus: 0
				)
			)
		)

		let result = try processSession.runObserved(using: driver)
		let report = try store.stateReport(id: "oci-demo")

		XCTAssertNotNil(result.runningSession.lifecycleStore)
		XCTAssertEqual(report.status, .stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.exitStatus, 0)
		XCTAssertEqual(report.bundle, "/bundles/oci-demo")
		XCTAssertEqual(
			driver.events,
			[
				"start:created:nil",
				"wait:running:42"
			]
		)
	}

	func testOCIRuntimeLifecycleControllerRejectsInvalidObservedLinuxExitStatus() throws {
		XCTAssertThrowsError(try OrlixOCIRuntimeProcessExitObservation(pid: 42, exitStatus: 256)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidExitStatus(256)
			)
		}

		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let configured = OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		let running = try configured.create().start(pid: 42)

		XCTAssertThrowsError(try running.exit(exitStatus: -1)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.invalidExitStatus(-1)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRejectsInvalidStartPID() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let created = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		.create()

		for invalidPID in [Int32(0), Int32(-1)] {
			XCTAssertThrowsError(try created.start(pid: invalidPID)) { error in
				XCTAssertEqual(
					error as? OrlixOCIRuntimeLifecycleError,
					.invalidPID(invalidPID)
				)
			}
		}
	}

	func testOCIRuntimeLifecycleControllerProducesStateReportJSON() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(
			Data(
				"""
				{
				  "ociVersion": "1.1.0",
				  "annotations": { "org.opencontainers.image.ref.name": "orlix-demo" },
				  "process": { "args": ["/bin/sh"], "cwd": "/" }
				}
				""".utf8
			)
		)
		let stopped = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		.create()
		.start(pid: 42)
		.exit(
			observedSignal: OrlixOCIRuntimeProcessSignalObservation(
				pid: 42,
				signal: 15
			)
		)

		let report = try stopped.stateReport()
		XCTAssertEqual(report.ociVersion, "1.1.0")
		XCTAssertEqual(report.id, "oci-demo")
		XCTAssertEqual(report.status, .stopped)
		XCTAssertEqual(report.pid, 42)
		XCTAssertEqual(report.bundle, "/bundles/oci-demo")
		XCTAssertEqual(report.annotations["org.opencontainers.image.ref.name"], "orlix-demo")
		XCTAssertEqual(report.exitStatus, 143)

		let json = String(decoding: try report.jsonData(), as: UTF8.self)
		XCTAssertTrue(json.contains(#""bundle" : "/bundles/oci-demo""#))
		XCTAssertTrue(json.contains(#""id" : "oci-demo""#))
		XCTAssertTrue(json.contains(#""status" : "stopped""#))
		XCTAssertTrue(json.contains(#""pid" : 42"#))

		let decoded = try JSONDecoder().decode(
			OrlixOCIRuntimeStateReport.self,
			from: try report.jsonData()
		)
		XCTAssertEqual(decoded, report)
	}

	func testOCIRuntimeLifecycleControllerRejectsUnavailableStateReports() throws {
		let configured = OrlixOCIRuntimeLifecycleController(
			config: try OrlixOCIRuntimeConfigParser().parse(minimalOCIRuntimeConfig()),
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)

		XCTAssertThrowsError(try configured.stateReport()) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.configured)
			)
		}

		let deleted = try configured.delete()
		XCTAssertThrowsError(try deleted.stateReport()) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.deleted)
			)
		}
	}

func testOCIRuntimeConfigParserTranslatesSupportedDocumentsBindMount() throws {
	let config = Data(
		#"{"ociVersion":"1.1.0","process":{"args":["/bin/sh"],"cwd":"/"},"root":{"path":"rootfs"},"mounts":[{"destination":"/home/root/Documents","type":"bind","source":"orlix:documents","options":["rbind","ro","nosuid","nodev","noexec"]},{"destination":"/mnt/shared","type":"bind","source":"orlix:documents","options":["bind","rw"]}]}"#.utf8
	)
		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
        XCTAssertEqual(descriptor.mounts.count, 2)
        XCTAssertEqual(descriptor.mounts[0].destination, "/home/root/Documents")
        XCTAssertEqual(descriptor.mounts[0].type, "bind")
        XCTAssertEqual(descriptor.mounts[0].source, "orlix:documents")
        XCTAssertEqual(descriptor.mounts[1].destination, "/mnt/shared")
        XCTAssertEqual(descriptor.mounts[1].type, "bind")
        XCTAssertEqual(descriptor.mounts[1].source, "orlix:documents")

		let environment = try descriptor.environmentDescriptor(
			id: "oci-bind-mounts",
			rootMount: .defaultOverlay
		)
        XCTAssertEqual(environment.mounts.count, 2)
	XCTAssertEqual(environment.mounts[0].source, .documents)
	XCTAssertEqual(environment.mounts[0].targetPath, "/home/root/Documents")
	XCTAssertTrue(environment.mounts[0].readOnly)
	XCTAssertTrue(environment.mounts[0].noExec)
	XCTAssertEqual(environment.mounts[1].source, .documents)
	XCTAssertEqual(environment.mounts[1].targetPath, "/mnt/shared")
	XCTAssertFalse(environment.mounts[1].readOnly)
	XCTAssertFalse(environment.mounts[1].noExec)
}

func testOCIRuntimeConfigParserTranslatesExternalBookmarkBindMount() throws {
	let config = Data(
		#"{"ociVersion":"1.1.0","process":{"args":["/bin/sh"],"cwd":"/"},"root":{"path":"rootfs"},"mounts":[{"destination":"/mnt/project","type":"bind","source":"orlix:external:selected-project","options":["rbind","ro"]}]}"#.utf8
	)
		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
		XCTAssertEqual(descriptor.mounts.count, 1)
		XCTAssertEqual(descriptor.mounts[0].destination, "/mnt/project")
		XCTAssertEqual(descriptor.mounts[0].type, "bind")
		XCTAssertEqual(descriptor.mounts[0].source, "orlix:external:selected-project")

		let environment = try descriptor.environmentDescriptor(
			id: "oci-external-bind-mount",
			rootMount: .defaultOverlay
		)
		XCTAssertEqual(environment.mounts.count, 1)
		XCTAssertEqual(
			environment.mounts[0].source,
			.securityScopedExternal(bookmarkID: "selected-project")
		)
	XCTAssertEqual(environment.mounts[0].targetPath, "/mnt/project")
	XCTAssertTrue(environment.mounts[0].readOnly)
}

func testOCIRuntimeConfigParserTranslatesStandardHostPathBindMount() throws {
	let root = temporaryRegistryRoot()
	let hostDirectory = root.appendingPathComponent("oci-host-source", isDirectory: true)
	try FileManager.default.createDirectory(
		at: hostDirectory,
		withIntermediateDirectories: true
	)
	let config = Data(
		"""
		{
		  "ociVersion": "1.1.0",
		  "process": { "args": ["/bin/sh"], "cwd": "/" },
		  "root": { "path": "rootfs" },
		  "mounts": [
		    {
		      "destination": "/mnt/oci-host",
		      "type": "bind",
		      "source": "\(hostDirectory.path)",
		      "options": ["rbind", "ro", "noexec"]
		    }
		  ]
		}
		""".utf8
	)
	let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
	XCTAssertEqual(descriptor.mounts.count, 1)
	XCTAssertEqual(descriptor.mounts[0].destination, "/mnt/oci-host")
	XCTAssertEqual(descriptor.mounts[0].type, "bind")
	XCTAssertEqual(descriptor.mounts[0].source, hostDirectory.path)

	let environment = try descriptor.environmentDescriptor(
		id: "oci-host-bind-mount",
		rootMount: .defaultOverlay
	)
	XCTAssertEqual(environment.mounts.count, 1)
	XCTAssertEqual(environment.mounts[0].source, .hostPath(hostDirectory.path))
	XCTAssertEqual(environment.mounts[0].targetPath, "/mnt/oci-host")
	XCTAssertTrue(environment.mounts[0].readOnly)
	XCTAssertTrue(environment.mounts[0].noExec)

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: root.appendingPathComponent("Application Support/Orlix"),
		cacheRoot: root.appendingPathComponent("Caches/Orlix"),
		scratchRoot: root.appendingPathComponent("tmp/Orlix")
	)
	let layout = try registry.prepareStorage(
		for: environment,
		fileManager: .default
	)
	try Data("base".utf8).write(to: layout.baseImageURL)
	try Data("state".utf8).write(to: layout.stateImageURL)
	let rootImage = try OrlixEnvironmentRootImage.materialized(
		descriptor: environment,
		layout: layout
	)
	XCTAssertEqual(
		rootImage.hostDirectories,
		[
			OrlixHostDirectoryRegistration(
				identifier: "orlix-host0",
				hostPath: hostDirectory.path,
				readOnly: true
			)
		]
	)
	let commandLine = try XCTUnwrap(rootImage.bootConfig.kernelCommandLine)
	XCTAssertTrue(commandLine.contains("orlix.mount.host0.target=/mnt/oci-host"))
	XCTAssertTrue(commandLine.contains("orlix.mount.host0.readonly=1"))
		XCTAssertTrue(commandLine.contains("orlix.mount.host0.noexec=1"))
		XCTAssertFalse(commandLine.contains(hostDirectory.path))
	}

	func testOCIRuntimeConfigParserTranslatesTmpfsMountOptions() throws {
		let config = Data(
			"""
			{
			  "ociVersion": "1.1.0",
			  "process": { "args": ["/bin/sh"], "cwd": "/" },
			  "root": { "path": "rootfs" },
			  "mounts": [
			    {
			      "destination": "/run/oci-cache",
			      "type": "tmpfs",
			      "source": "tmpfs",
			      "options": ["nosuid", "nodev", "noexec", "ro", "size=64m", "mode=0755", "uid=0", "gid=0"]
			    }
			  ]
			}
			""".utf8
		)

		let descriptor = try OrlixOCIRuntimeConfigParser().parse(config)
		XCTAssertEqual(descriptor.mounts.count, 1)
		XCTAssertEqual(descriptor.mounts[0].destination, "/run/oci-cache")
		XCTAssertEqual(descriptor.mounts[0].type, "tmpfs")

		let environment = try descriptor.environmentDescriptor(
			id: "oci-tmpfs-mount",
			rootMount: .defaultOverlay
		)
		XCTAssertTrue(environment.mounts.isEmpty)
		XCTAssertEqual(
			environment.tmpfsMounts,
			[
				try OrlixEnvironmentTmpfsMount(
					targetPath: "/run/oci-cache",
					readOnly: true,
					noSuid: true,
					noDev: true,
					noExec: true,
					data: "size=64m,mode=0755,uid=0,gid=0"
				)
			]
		)
	}

	func testOCIRuntimeConfigParserRejectsUnsupportedMounts() throws {
	let unsupportedMountConfigs: [(String, OrlixOCIRuntimeConfigError)] = [
			(
				#"{ "destination": "/cgroup", "type": "cgroup2", "source": "cgroup2" }"#,
				.unsupportedLinuxFeature("mounts.destination")
			),
			(
				#"{ "destination": "/sys/fs/cgroup", "type": "cgroup2", "source": "not-cgroup2" }"#,
				.unsupportedLinuxFeature("mounts.source")
			),
			(
				#"{ "destination": "/sys/fs/cgroup", "type": "cgroup2", "source": "cgroup2", "options": ["rw"] }"#,
				.unsupportedLinuxFeature("mounts.options")
			),
			(
				#"{ "destination": "relative", "type": "tmpfs", "source": "tmpfs" }"#,
				.unsupportedLinuxFeature("mounts.destination")
			),
			(
				#"{ "destination": "/proc/oci-cache", "type": "tmpfs", "source": "tmpfs" }"#,
				.unsupportedLinuxFeature("mounts.destination")
			),
			(
				#"{ "destination": "/run", "type": "tmpfs", "source": "tmpfs", "options": ["exec"] }"#,
				.unsupportedLinuxFeature("mounts.options")
			),
			(
				#"{ "destination": "/run", "type": "tmpfs", "source": "tmpfs", "options": ["mode=999"] }"#,
				.unsupportedLinuxFeature("mounts.options")
			),
			(
				#"{ "destination": "/proc", "type": "proc", "source": "not-proc" }"#,
				.unsupportedLinuxFeature("mounts.source")
			),
			(
				#"{ "destination": "/proc", "type": "proc", "source": "proc", "options": ["nosuid"] }"#,
				.unsupportedLinuxFeature("mounts.options")
			),
		(
		#"{ "destination": "/mnt/host", "type": "bind", "source": "/Users/rudi/../Documents", "options": ["rbind"] }"#,
		.unsupportedLinuxFeature("mounts.source")
	),
		(
			#"{ "destination": "/mnt/external", "type": "bind", "source": "orlix:security-scoped:selected-folder", "options": ["bind"] }"#,
			.unsupportedLinuxFeature("mounts.source")
		),
		(
			#"{ "destination": "/mnt/host", "type": "bind", "source": "orlix:documents", "options": ["rshared"] }"#,
			.unsupportedLinuxFeature("mounts.options")
			)
		]

		for (mountFragment, expectedError) in unsupportedMountConfigs {
			let config = Data(
				"""
				{
				  "ociVersion": "1.1.0",
				  "process": { "args": ["/bin/sh"], "cwd": "/" },
				  "mounts": [\(mountFragment)]
				}
				""".utf8
			)

			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
				XCTAssertEqual(error as? OrlixOCIRuntimeConfigError, expectedError)
			}
		}
	}

	func testOCIRuntimeLifecycleControllerProducesSessionDescriptor() throws {
		let config = try OrlixOCIRuntimeConfigParser().parse(Data("""
		{
		  "ociVersion" : "1.1.0",
		  "root" : {
		    "path" : "rootfs"
		  },
		  "process" : {
		    "terminal" : true,
		    "consoleSize" : { "height" : 24, "width" : 80 },
		    "args" : ["/usr/bin/env", "sh"],
		    "env" : [
		      "HOME=/root",
		      "PATH=/usr/bin:/bin",
		      "TERM=xterm-256color"
		    ],
		    "cwd" : "/work",
		    "user" : {
		      "uid" : 1000,
		      "gid" : 1000
		    }
		  }
		}
		""".utf8))
		let controller = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-session-created",
			bundlePath: "/bundles/oci-session-created"
		).create()

		let session = try controller.sessionDescriptor(rootMount: OrlixEnvironmentRootMount.defaultOverlay)

		XCTAssertEqual(session.id, "oci-session-created")
		XCTAssertEqual(session.lifecycleState, OrlixOCIRuntimeLifecycleState.created)
		XCTAssertTrue(session.terminal)
		XCTAssertEqual(
			session.consoleSize,
			OrlixOCIRuntimeConsoleSize(height: 24, width: 80)
		)
		XCTAssertEqual(session.environment.id, "oci-session-created")
		XCTAssertEqual(session.environment.source, OrlixEnvironmentSource.ociLayout)
		XCTAssertEqual(session.environment.rootImageIdentifier, "oci-session-created")
		XCTAssertEqual(session.environment.defaultCommand, ["/usr/bin/env", "sh"])
		XCTAssertEqual(session.environment.defaultEnvironment["HOME"], "/root")
		XCTAssertEqual(session.environment.defaultEnvironment["PATH"], "/usr/bin:/bin")
		XCTAssertEqual(session.environment.defaultEnvironment["TERM"], "xterm-256color")
		XCTAssertEqual(session.environment.defaultWorkingDirectory, "/work")
		XCTAssertEqual(session.environment.defaultUserID, 1000)
		XCTAssertEqual(session.environment.defaultGroupID, 1000)
		XCTAssertEqual(session.environment.rootMount, OrlixEnvironmentRootMount.defaultOverlay)
		XCTAssertTrue(session.environment.mounts.isEmpty)
	}

	func testOCIRuntimeSessionDescriptorLaunchesThroughEnvironmentRegistry() throws {
		let root = FileManager.default.temporaryDirectory.appendingPathComponent(
			"orlix-oci-session-\(UUID().uuidString)",
			isDirectory: true
		)
		defer { try? FileManager.default.removeItem(at: root) }

		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: root.appendingPathComponent("Application Support/Orlix"),
			cacheRoot: root.appendingPathComponent("Caches/Orlix"),
			scratchRoot: root.appendingPathComponent("tmp/Orlix")
		)
		let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
		let controller = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-session-registry",
			bundlePath: "/bundles/oci-session-registry"
		).create()
		let sessionDescriptor = try controller.sessionDescriptor(rootMount: OrlixEnvironmentRootMount.defaultOverlay)
		let layout = try registry.layout(forEnvironmentID: sessionDescriptor.environment.id)
		let encoder = JSONEncoder()
		encoder.outputFormatting = [.prettyPrinted, .sortedKeys]

		try FileManager.default.createDirectory(
			at: layout.rootDirectory,
			withIntermediateDirectories: true
		)
		try encoder
			.encode(sessionDescriptor.environment)
			.write(to: try registry.descriptorURL(forEnvironmentID: sessionDescriptor.environment.id))
		try Data().write(to: layout.baseImageURL)
		try Data().write(to: layout.stateImageURL)

		let linuxSession = try OrlixLinuxSession(
			ociRuntimeSession: sessionDescriptor,
			registry: registry
		)

		let materializedRootImage = try XCTUnwrap(linuxSession.materializedRootImageForTesting)
		XCTAssertEqual(materializedRootImage.environmentID, sessionDescriptor.environment.id)
		XCTAssertEqual(materializedRootImage.rootImageIdentifier, sessionDescriptor.environment.rootImageIdentifier)
		XCTAssertEqual(materializedRootImage.baseImageURL, layout.baseImageURL)
		XCTAssertEqual(materializedRootImage.stateImageURL, layout.stateImageURL)
		let commandLine = try XCTUnwrap(linuxSession.bootConfig.kernelCommandLine)
		XCTAssertTrue(commandLine.hasPrefix("orlix.terminal=1 "))
		XCTAssertTrue(commandLine.contains("console=hvc0"))
		XCTAssertTrue(commandLine.contains("orlix.exec=/usr/bin/env"))
		XCTAssertTrue(commandLine.contains("orlix.argv0=/usr/bin/env"))
		XCTAssertTrue(commandLine.contains("orlix.argv1=sh"))
		XCTAssertTrue(commandLine.contains("orlix.argv2=-lc"))
		XCTAssertTrue(commandLine.contains("orlix.env0=HOME=/root"))
		XCTAssertTrue(commandLine.contains("orlix.env1=PATH=/usr/bin:/bin"))
	XCTAssertTrue(commandLine.contains("orlix.env2=TERM=xterm-256color"))
	XCTAssertTrue(commandLine.contains("orlix.cwd=/work"))
	XCTAssertTrue(commandLine.contains("orlix.uid=1000"))
	XCTAssertTrue(commandLine.contains("orlix.gid=1000"))
	XCTAssertFalse(commandLine.contains("orlix.terminal=0"))
}

func testOCIRuntimeSessionDescriptorCarriesTerminalFalseIntoBootCommandLine() throws {
	let root = FileManager.default.temporaryDirectory.appendingPathComponent(
		"orlix-oci-session-terminal-false-\(UUID().uuidString)",
		isDirectory: true
	)
	defer { try? FileManager.default.removeItem(at: root) }

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: root.appendingPathComponent("Application Support/Orlix"),
		cacheRoot: root.appendingPathComponent("Caches/Orlix"),
		scratchRoot: root.appendingPathComponent("tmp/Orlix")
	)
	let config = try OrlixOCIRuntimeConfigParser().parse(Data("""
	{
		"ociVersion" : "1.1.0",
		"root" : { "path" : "rootfs" },
		"process" : {
			"terminal" : false,
			"args" : ["/bin/true"],
			"cwd" : "/"
		}
	}
	""".utf8))
	let controller = try OrlixOCIRuntimeLifecycleController(
		config: config,
		id: "oci-session-terminal-false",
		bundlePath: "/bundles/oci-session-terminal-false"
	).create()
	let sessionDescriptor = try controller.sessionDescriptor(
		rootMount: OrlixEnvironmentRootMount.defaultOverlay
	)
	XCTAssertFalse(sessionDescriptor.terminal)

	try registry.save(sessionDescriptor.environment)
	let layout = try registry.layout(forEnvironmentID: sessionDescriptor.environment.id)
	try Data().write(to: layout.baseImageURL)
	try Data().write(to: layout.stateImageURL)

	let linuxSession = try OrlixLinuxSession(
		ociRuntimeSession: sessionDescriptor,
		registry: registry
	)
		let commandLine = try XCTUnwrap(linuxSession.bootConfig.kernelCommandLine)
		XCTAssertTrue(commandLine.contains("orlix.exec=/bin/true"))
	XCTAssertTrue(commandLine.contains("orlix.terminal=0"))
	XCTAssertTrue(commandLine.hasPrefix("orlix.terminal=0 "))
}

func testOCIRuntimeBundleLinuxSessionUsesMaterializedRootAndTerminalFalseMetadata() throws {
	let fileManager = FileManager.default
	let root = fileManager.temporaryDirectory.appendingPathComponent(
		"orlix-oci-bundle-linux-session-\(UUID().uuidString)",
		isDirectory: true
	)
	defer { try? fileManager.removeItem(at: root) }
	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: root.appendingPathComponent("Application Support/Orlix"),
		cacheRoot: root.appendingPathComponent("Caches/Orlix"),
		scratchRoot: root.appendingPathComponent("tmp/Orlix")
	)
	let bundleURL = root.appendingPathComponent("bundle", isDirectory: true)
	try fileManager.createDirectory(
		at: bundleURL.appendingPathComponent("rootfs", isDirectory: true),
		withIntermediateDirectories: true
	)
	try Data("""
	{
		"ociVersion" : "1.1.0",
		"root" : { "path" : "rootfs" },
		"process" : {
			"terminal" : false,
			"args" : ["/bin/true"],
			"cwd" : "/"
		}
	}
	""".utf8).write(to: bundleURL.appendingPathComponent("config.json"))
	let layout = try registry.prepareStorage(forEnvironmentID: "oci-bundle-session")
	try Data("base".utf8).write(to: layout.baseImageURL)
	try Data("state".utf8).write(to: layout.stateImageURL)
	let session = try OrlixLinuxSession(
		ociRuntimeBundle: try OrlixOCIRuntimeBundle.load(from: bundleURL),
		id: "oci-bundle-session",
		rootMount: .defaultOverlay,
		registry: registry,
		terminal: OrlixTerminalSession()
	)
	let rootImage = try XCTUnwrap(session.materializedRootImageForTesting)
	XCTAssertEqual(rootImage.baseImageURL, layout.baseImageURL)
	XCTAssertEqual(rootImage.stateImageURL, layout.stateImageURL)
	XCTAssertEqual(
		try registry.load(environmentID: "oci-bundle-session").defaultCommand,
		["/bin/true"]
	)
	let commandLine = try XCTUnwrap(session.bootConfig.kernelCommandLine)
	XCTAssertTrue(commandLine.contains("orlix.exec=/bin/true"))
	XCTAssertTrue(commandLine.hasPrefix("orlix.terminal=0 "))
}

func testOCIRuntimeLifecycleControllerRejectsStoppedSessionDescriptor() throws {
	let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
	let stopped = try OrlixOCIRuntimeLifecycleController(
			config: config,
			id: "oci-demo",
			bundlePath: "/bundles/oci-demo"
		)
		.create()
		.start(pid: 42)
		.exit(exitStatus: 0)

		XCTAssertThrowsError(try stopped.sessionDescriptor(rootMount: OrlixEnvironmentRootMount.defaultOverlay)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.stopped)
			)
		}
	}

	func testOCIRuntimeLifecycleControllerRejectsUnavailableSessionDescriptors() throws {
		let configured = OrlixOCIRuntimeLifecycleController(
			config: try OrlixOCIRuntimeConfigParser().parse(minimalOCIRuntimeConfig()),
			id: "oci-session-configured",
			bundlePath: "/bundles/oci-session-configured"
		)
		XCTAssertThrowsError(try configured.sessionDescriptor(rootMount: OrlixEnvironmentRootMount.defaultOverlay)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.configured)
			)
		}

		let deleted = try configured.delete()
		XCTAssertThrowsError(try deleted.sessionDescriptor(rootMount: OrlixEnvironmentRootMount.defaultOverlay)) { error in
			XCTAssertEqual(
				error as? OrlixOCIRuntimeLifecycleError,
				.stateUnavailable(.deleted)
			)
	}
}

private func makeCreatedOCIRuntimeProcessSessionFixture(
	scratchName: String
) throws -> (
	session: OrlixOCIRuntimeProcessSession,
	scratch: URL,
	terminal: RecordingTerminalTransport
) {
	let fileManager = FileManager.default
	let scratch = fileManager.temporaryDirectory.appendingPathComponent(
		"\(scratchName)-\(UUID().uuidString)",
		isDirectory: true
	)
	try fileManager.createDirectory(
		at: scratch,
		withIntermediateDirectories: true
	)
	let stateRoot = scratch.appendingPathComponent("state", isDirectory: true)
	let cacheRoot = scratch.appendingPathComponent("cache", isDirectory: true)
	let scratchRoot = scratch.appendingPathComponent("scratch", isDirectory: true)
	try fileManager.createDirectory(
		at: stateRoot.appendingPathComponent("environments/oci-demo", isDirectory: true),
		withIntermediateDirectories: true
	)
	try fileManager.createDirectory(at: cacheRoot, withIntermediateDirectories: true)
	try fileManager.createDirectory(at: scratchRoot, withIntermediateDirectories: true)

	let registry = OrlixEnvironmentRegistry(
		linuxStateRoot: stateRoot,
		cacheRoot: cacheRoot,
		scratchRoot: scratchRoot
	)
	let config = try OrlixOCIRuntimeConfigParser().parse(nonRootOCIRuntimeConfig())
	let configured = OrlixOCIRuntimeLifecycleController(
		config: config,
		id: "oci-demo",
		bundlePath: "/bundles/oci-demo"
	)
	let processHandle = try OrlixOCIRuntimeProcessHandle(
		lifecycle: try configured.create(),
		rootMount: OrlixEnvironmentRootMount.defaultOverlay
	)
	try registry.save(processHandle.sessionDescriptor.environment)
	try Data("base".utf8).write(
		to: stateRoot.appendingPathComponent("environments/oci-demo/base.ext4")
	)
	try Data("state".utf8).write(
		to: stateRoot.appendingPathComponent("environments/oci-demo/state.ext4")
	)
	let terminal = RecordingTerminalTransport()
	let linuxSession = try OrlixLinuxSession(
		ociRuntimeSession: processHandle.sessionDescriptor,
		registry: registry,
		terminal: OrlixTerminalSession(transport: terminal)
	)

	return (
		OrlixOCIRuntimeProcessSession(
			processHandle: processHandle,
			linuxSession: linuxSession
		),
		scratch,
		terminal
	)
}

private func descriptor(
_ descriptor: OrlixEnvironmentDescriptor,
healthcheck: OrlixEnvironmentHealthcheck?
) -> OrlixEnvironmentDescriptor {
OrlixEnvironmentDescriptor(
id: descriptor.id,
source: descriptor.source,
platform: descriptor.platform,
rootImageIdentifier: descriptor.rootImageIdentifier,
defaultCommand: descriptor.defaultCommand,
defaultEnvironment: descriptor.defaultEnvironment,
defaultWorkingDirectory: descriptor.defaultWorkingDirectory,
defaultUserID: descriptor.defaultUserID,
defaultGroupID: descriptor.defaultGroupID,
defaultSupplementaryGroups: descriptor.defaultSupplementaryGroups,
defaultCapabilities: descriptor.defaultCapabilities,
defaultNoNewPrivileges: descriptor.defaultNoNewPrivileges,
defaultCloseAdditionalFds: descriptor.defaultCloseAdditionalFds,
defaultStopSignal: descriptor.defaultStopSignal,
defaultTerminal: descriptor.defaultTerminal,
defaultTerminalRows: descriptor.defaultTerminalRows,
defaultTerminalColumns: descriptor.defaultTerminalColumns,
defaultOOMScoreAdjustment: descriptor.defaultOOMScoreAdjustment,
defaultScheduler: descriptor.defaultScheduler,
defaultIOPriority: descriptor.defaultIOPriority,
defaultCPUAffinity: descriptor.defaultCPUAffinity,
defaultUmask: descriptor.defaultUmask,
defaultRlimits: descriptor.defaultRlimits,
defaultPersonalityDomain: descriptor.defaultPersonalityDomain,
hostname: descriptor.hostname,
domainname: descriptor.domainname,
rootMount: descriptor.rootMount,
rootReadonly: descriptor.rootReadonly,
rootPropagation: descriptor.rootPropagation,
sysctls: descriptor.sysctls,
maskedPaths: descriptor.maskedPaths,
readonlyPaths: descriptor.readonlyPaths,
cgroupsPath: descriptor.cgroupsPath,
cgroupPidsLimit: descriptor.cgroupPidsLimit,
cgroupCPUMax: descriptor.cgroupCPUMax,
cgroupCPUWeight: descriptor.cgroupCPUWeight,
cgroupMemoryMax: descriptor.cgroupMemoryMax,
cgroupIOWeight: descriptor.cgroupIOWeight,
cgroupUnified: descriptor.cgroupUnified,
deviceNodes: descriptor.deviceNodes,
timeOffsets: descriptor.timeOffsets,
uidMappings: descriptor.uidMappings,
gidMappings: descriptor.gidMappings,
namespaces: descriptor.namespaces,
namespacePaths: descriptor.namespacePaths,
tmpfsMounts: descriptor.tmpfsMounts,
mounts: descriptor.mounts,
exposedPorts: descriptor.exposedPorts,
publishedPorts: descriptor.publishedPorts,
imageVolumes: descriptor.imageVolumes,
healthcheck: healthcheck,
annotations: descriptor.annotations
)
}

private enum RecordingOCIRuntimeProcessObservationDriverError: Error, Equatable {
	case requestedStartFailure
	case requestedSignalFailure
	case requestedWaitFailure
}

private final class RecordingOCIRuntimeProcessObservationDriver: OrlixOCIRuntimeProcessObservationDriver, @unchecked Sendable {
	private let startObservation: OrlixOCIRuntimeProcessStartObservation
	private let completion: OrlixOCIRuntimeProcessCompletionObservation
	private let failsOnStart: Bool
	private let failsOnSignal: Bool
	private let failsOnWait: Bool
private(set) var events: [String] = []
private(set) var startCommands: [[String]] = []
private(set) var startRootImageIdentifiers: [String] = []
private(set) var startKernelCommandLines: [String?] = []
private(set) var startConsoleSizes: [OrlixOCIRuntimeConsoleSize?] = []
private(set) var waitRootImageIdentifiers: [String] = []

	init(startPID: Int32,
	     completion: OrlixOCIRuntimeProcessCompletionObservation,
	     failsOnStart: Bool = false,
	     failsOnSignal: Bool = false,
	     failsOnWait: Bool = false) throws
	{
		self.startObservation = try OrlixOCIRuntimeProcessStartObservation(
			pid: startPID
		)
		self.completion = completion
		self.failsOnStart = failsOnStart
		self.failsOnSignal = failsOnSignal
		self.failsOnWait = failsOnWait
	}

	func start(processSession: OrlixOCIRuntimeProcessSession) throws -> OrlixOCIRuntimeProcessStartObservation {
		startCommands.append(
			processSession.processHandle.sessionDescriptor.environment.defaultCommand
		)
startRootImageIdentifiers.append(
processSession.processHandle.sessionDescriptor.environment.rootImageIdentifier
)
startKernelCommandLines.append(
processSession.linuxSession.bootConfig.kernelCommandLine
)
startConsoleSizes.append(processSession.processHandle.sessionDescriptor.consoleSize)
events.append(
			"start:\(processSession.processHandle.lifecycle.record.state):\(String(describing: processSession.processHandle.lifecycle.record.pid))"
		)
		if failsOnStart {
			throw RecordingOCIRuntimeProcessObservationDriverError.requestedStartFailure
		}
		return startObservation
	}

	func signal(processSession: OrlixOCIRuntimeProcessSession, signal: Int32) throws {
		events.append(
			"signal:\(processSession.processHandle.lifecycle.record.state):\(processSession.processHandle.lifecycle.record.pid ?? 0):\(signal)"
		)
		if failsOnSignal {
			throw RecordingOCIRuntimeProcessObservationDriverError.requestedSignalFailure
		}
	}

	func wait(processSession: OrlixOCIRuntimeProcessSession) throws -> OrlixOCIRuntimeProcessCompletionObservation {
		waitRootImageIdentifiers.append(
			processSession.processHandle.sessionDescriptor.environment.rootImageIdentifier
		)
		events.append(
			"wait:\(processSession.processHandle.lifecycle.record.state):\(processSession.processHandle.lifecycle.record.pid ?? 0)"
		)
		if failsOnWait {
			throw RecordingOCIRuntimeProcessObservationDriverError.requestedWaitFailure
		}
		return completion
	}
}

	private func minimalOCIRuntimeConfig() -> Data {
		Data(
			"""
			{
			  "ociVersion": "1.1.0",
			  "process": {
			    "args": ["/bin/sh"],
			    "cwd": "/"
			  }
			}
			""".utf8
		)
	}

	private func nonRootOCIRuntimeConfig(rootPath: String = "rootfs") -> Data {
		Data(
			"""
			{
			  "ociVersion": "1.1.0",
			  "root": {
			    "path": "\(rootPath)"
			  },
			  "process": {
			    "terminal": true,
			    "args": ["/usr/bin/env", "sh", "-lc"],
			    "env": [
			      "HOME=/root",
			      "PATH=/usr/bin:/bin",
			      "TERM=xterm-256color"
			    ],
			    "cwd": "/work",
			    "user": {
			      "uid": 1000,
			      "gid": 1000
			    }
			  }
			}
			""".utf8
		)
	}

	func testOCIRuntimeConfigParserRejectsInvalidProcessSurface() throws {
		let invalidConfigs: [(Data, OrlixOCIRuntimeConfigError)] = [
			(
				Data(#"{ "ociVersion": "1.1.0" }"#.utf8),
				.missingProcess
			),
			(
				Data(#"{ "ociVersion": "1.1.0", "process": { "args": [], "cwd": "/" } }"#.utf8),
				.emptyProcessArgs
			),
		(
			Data(#"{ "ociVersion": "1.1.0", "process": { "args": ["/bin/sh"], "env": ["BAD"], "cwd": "/" } }"#.utf8),
			.invalidEnvironmentEntry("BAD")
		),
		(
			Data(#"{ "ociVersion": "1.1.0", "root": { "path": "rootfs" }, "process": { "args": ["/bin/sh"] } }"#.utf8),
			.missingWorkingDirectory
		),
		(
			Data(#"{ "ociVersion": "1.1.0", "process": { "args": ["/bin/sh"], "cwd": "relative" } }"#.utf8),
			.invalidWorkingDirectory("relative")
		)
		]

		for (config, expectedError) in invalidConfigs {
			XCTAssertThrowsError(try OrlixOCIRuntimeConfigParser().parse(config)) { error in
				XCTAssertEqual(error as? OrlixOCIRuntimeConfigError, expectedError)
			}
		}
	}
}

private actor RecordingOCIRegistryFetch {
	private var storage: [String: [OrlixOCIRegistryFetchResponse]]
	private let defaultResponse: OrlixOCIRegistryFetchResponse?
	private var recordedRequests: [OrlixOCIRegistryFetchRequest] = []

	init(responses: [String: OrlixOCIRegistryFetchResponse]) {
		self.storage = responses.mapValues { [$0] }
		self.defaultResponse = nil
	}

	init(scriptedResponses: [String: [OrlixOCIRegistryFetchResponse]]) {
		self.storage = scriptedResponses
		self.defaultResponse = nil
	}

	init(defaultResponse: OrlixOCIRegistryFetchResponse) {
		self.storage = [:]
		self.defaultResponse = defaultResponse
	}

	var requests: [OrlixOCIRegistryFetchRequest] {
		recordedRequests
	}

	func fetch(
		_ request: OrlixOCIRegistryFetchRequest
	) async throws -> OrlixOCIRegistryFetchResponse {
		recordedRequests.append(request)
		if var responses = storage[request.url.absoluteString],
		   let response = responses.first {
			responses.removeFirst()
			storage[request.url.absoluteString] = responses
			return response
		}
		if let defaultResponse {
			return defaultResponse
		}
		return OrlixOCIRegistryFetchResponse(statusCode: 404, headers: [:], body: Data())
	}
}

private enum RecordingMaterializationCommandRunnerError: Error, Equatable {
	case requestedFailure(OrlixEnvironmentImageMaterializationCommand)
}

private final class RecordingMaterializationCommandRunner:
	OrlixEnvironmentImageMaterializationCommandRunner,
	@unchecked Sendable
{
	private let failingCommandIndex: Int?
	private let createsPlaceholderImagesForTruncateCommands: Bool
	private(set) var commands: [OrlixEnvironmentImageMaterializationCommand] = []

	init(
		failingCommandIndex: Int? = nil,
		createsPlaceholderImagesForTruncateCommands: Bool = false
	) {
		self.failingCommandIndex = failingCommandIndex
		self.createsPlaceholderImagesForTruncateCommands = createsPlaceholderImagesForTruncateCommands
	}

	func run(_ command: OrlixEnvironmentImageMaterializationCommand) throws {
		if commands.count == failingCommandIndex {
			throw RecordingMaterializationCommandRunnerError.requestedFailure(command)
		}
		if createsPlaceholderImagesForTruncateCommands,
			command.executable.contains("truncate"),
			let imagePath = command.arguments.last
		{
			let imageURL = URL(fileURLWithPath: imagePath, isDirectory: false)
			try FileManager.default.createDirectory(
				at: imageURL.deletingLastPathComponent(),
				withIntermediateDirectories: true
			)
			try Data("placeholder ext4 image\n".utf8).write(to: imageURL)
		}
		commands.append(command)
	}
}

private final class RecordingPublicOCIInstallerCommandRunner: @unchecked Sendable {
	private(set) var executables: [URL] = []

	func run(executable: URL, arguments: [String]) throws {
		if executable.lastPathComponent.contains("truncate"),
		   let imagePath = arguments.last
		{
			let imageURL = URL(fileURLWithPath: imagePath, isDirectory: false)
			try FileManager.default.createDirectory(
				at: imageURL.deletingLastPathComponent(),
				withIntermediateDirectories: true
			)
			try Data("placeholder ext4 image\n".utf8).write(to: imageURL)
		}
		executables.append(executable)
	}
}

private final class DataRecorder: @unchecked Sendable {
	private let lock = NSLock()
	private var storage: [Data] = []

    var values: [Data] {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }

    func append(_ data: Data) {
        lock.lock()
        storage.append(data)
        lock.unlock()
    }
}

private final class RecordingTerminalTransport:
    OrlixTerminalTransport,
    @unchecked Sendable
{
    private var outputHandlers: [UUID: @Sendable (Data) -> Void] = [:]
    private(set) var sentInput: [Data] = []

    func attachOutput(
        _ handler: @escaping @Sendable (Data) -> Void
    ) -> OrlixTerminalOutput {
        let id = UUID()
        outputHandlers[id] = handler
        return OrlixTerminalOutput { [weak self] in
            self?.outputHandlers[id] = nil
        }
    }

func send(_ data: Data) {
sentInput.append(data)
}

func resize(rows: UInt32, columns: UInt32) {
sentInput.append(Data("\u{1B}]777;orlix.resize=\(rows)x\(columns)\u{7}".utf8))
}

func emit(_ data: Data) {
        for handler in outputHandlers.values {
            handler(data)
        }
    }
}
