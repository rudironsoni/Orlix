import Testing
@testable import Orlix

struct TSSHStartupPlanTests {
    @Test
    func freshPlainShellRestoresThePersistedWorkingDirectory() {
        let plan = planRestoringWorkingDirectory(
            .plainShell,
            workingDirectory: "/srv/project's",
            environment: .fallbackPOSIX
        )

        #expect(plan.command == "cd -- '/srv/project'\\''s'\n")
        #expect(plan.remoteSessionLifecycle == nil)
        #expect(!plan.mayExecuteUserStartupAction)
    }

    @Test
    func explicitStartupCommandIsNotReplacedByDirectoryRestore() {
        let original = TerminalShellStartupPlan(
            command: "start-session",
            remoteSessionLifecycle: nil,
            mayExecuteUserStartupAction: true
        )

        let plan = planRestoringWorkingDirectory(
            original,
            workingDirectory: "/srv/project",
            environment: .fallbackPOSIX
        )

        #expect(plan.command == "start-session")
        #expect(plan.mayExecuteUserStartupAction)
    }
}
