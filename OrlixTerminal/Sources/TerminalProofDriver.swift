import Foundation

final class TerminalProofDriver {
    private struct Step {
        let name: String
        let command: String
        let expectedOutput: String
    }

    private static let prompt = "sh-5.3#"

    private let sendInput: (Data) -> Void
    private let log: (String) -> Void
    private let steps: [Step]
    private var started = false
    private var failed = false
    private var stepIndex = 0
    private var preStartOutput = ""
    private var stepOutput = ""

    static func fromProcessArguments(
        sendInput: @escaping (Data) -> Void
    ) -> TerminalProofDriver? {
        fromArguments(ProcessInfo.processInfo.arguments, sendInput: sendInput)
    }

    static func fromArguments(
        _ arguments: [String],
        sendInput: @escaping (Data) -> Void
    ) -> TerminalProofDriver? {
        let enabled = arguments.contains {
            $0 == "--orlix-terminal-proof=shell-package"
        }
        guard enabled else {
            return nil
        }

        return TerminalProofDriver(sendInput: sendInput)
    }

    init(
        sendInput: @escaping (Data) -> Void,
        log: @escaping (String) -> Void = TerminalProofDriver.standardErrorLog
    ) {
        self.sendInput = sendInput
        self.log = log
        self.steps = [
            Step(
                name: "echo",
                command: #"printf '\117\122\114\111\130\137\105\103\110\117\012'"#,
                expectedOutput: "ORLIX_ECHO"
            ),
            Step(
                name: "pwd",
                command: #"/bin/test "$(/bin/pwd)" = / && printf '\120\127\104\137\117\113\012'"#,
                expectedOutput: "PWD_OK"
            ),
            Step(
                name: "cd",
                command: #"cd /tmp && printf '\103\104\137\117\113\012'"#,
                expectedOutput: "CD_OK"
            ),
            Step(
                name: "redirection",
                command: #"printf '\122\105\104\111\122\137\117\113\012' > /tmp/orlix-proof.txt; /bin/cat /tmp/orlix-proof.txt"#,
                expectedOutput: "REDIR_OK"
            ),
            Step(
                name: "pipe",
                command: #"printf '\120\111\120\105\137\117\113\012' | /bin/head -n 1"#,
                expectedOutput: "PIPE_OK"
            ),
            Step(
                name: "fork-exec-wait-status",
                command: #"sh -c 'exit 7'; printf 'status:%s\n' "$?""#,
                expectedOutput: "status:7"
            ),
            Step(
                name: "signal-trap",
                command: #"/bin/bash -c 'trap "printf \"\123\111\107\137\117\113\012\"; exit 0" TERM; kill -TERM $$; :'"#,
                expectedOutput: "SIG_OK"
            ),
            Step(
                name: "ls",
                command: #"/bin/ls /bin > /tmp/orlix-bin-list.txt && /bin/test -s /tmp/orlix-bin-list.txt && printf '\114\123\137\117\113\012'"#,
                expectedOutput: "LS_OK"
            ),
            Step(
                name: "ls-dot",
                command: #"cd /; /bin/ls . > /tmp/orlix-dot-list.txt && /bin/test -s /tmp/orlix-dot-list.txt && printf '\114\123\137\104\117\124\137\117\113\012'"#,
                expectedOutput: "LS_DOT_OK"
            ),
            Step(
                name: "cp-mv-ln",
                command: #"printf '\103\117\122\105\125\124\111\114\123\137\106\111\114\105\012' > /tmp/orlix-copy-source; /bin/cp /tmp/orlix-copy-source /tmp/orlix-copy; /bin/mv /tmp/orlix-copy /tmp/orlix-moved; /bin/ln /tmp/orlix-moved /tmp/orlix-linked; /bin/cat /tmp/orlix-linked"#,
                expectedOutput: "COREUTILS_FILE"
            ),
            Step(
                name: "sort-tail",
                command: #"printf '\142\145\164\141\012' > /tmp/orlix-sort; printf '\141\154\160\150\141\012' >> /tmp/orlix-sort; /bin/sort /tmp/orlix-sort | /bin/tail -n 1"#,
                expectedOutput: "beta"
            ),
            Step(
                name: "sleep-date",
                command: #"/bin/sleep 0; stamp=$(/bin/date -u +%s); /bin/test -n "$stamp" && printf '\104\101\124\105\137\117\113\012'"#,
                expectedOutput: "DATE_OK"
            ),
        ]
    }

    func receive(_ text: String) {
        guard !failed, stepIndex < steps.count else {
            return
        }

        if !started {
            preStartOutput += text
            if preStartOutput.count > 4096 {
                preStartOutput.removeFirst(preStartOutput.count - 4096)
            }
            guard preStartOutput.contains(Self.prompt) else {
                return
            }
            started = true
            preStartOutput.removeAll(keepingCapacity: false)
            log("ORLIX-TERMINAL-PROOF-BEGIN shell-package")
            sendCurrentStep()
            return
        }

        stepOutput += text
        let current = steps[stepIndex]
        let matchingOutput = Self.textForMatching(stepOutput)
        if matchingOutput.contains(current.expectedOutput),
           matchingOutput.contains(Self.prompt) {
            log("ORLIX-TERMINAL-PROOF-OK \(current.name)")
            stepIndex += 1
            stepOutput.removeAll(keepingCapacity: true)
            if stepIndex == steps.count {
                log("ORLIX-TERMINAL-PROOF-END shell-package status=0")
            } else {
                sendCurrentStep()
            }
        } else if matchingOutput.contains(Self.prompt) {
            failed = true
            log("ORLIX-TERMINAL-PROOF-FAIL \(current.name) reason=missing-output")
        }
    }

    private func sendCurrentStep() {
        guard stepIndex < steps.count else {
            return
        }
        let current = steps[stepIndex]
        guard let data = "\(current.command)\r".data(using: .utf8) else {
            failed = true
            log("ORLIX-TERMINAL-PROOF-FAIL \(current.name) reason=encoding")
            return
        }

        sendInput(data)
    }

    private static func standardErrorLog(_ line: String) {
        let text = "\(line)\n"
        if let data = text.data(using: .utf8) {
            FileHandle.standardError.write(data)
        }
    }

    private static func textForMatching(_ text: String) -> String {
        var output = ""
        var scalars = text.unicodeScalars.makeIterator()

        while let scalar = scalars.next() {
            if scalar.value != 0x1b {
                output.unicodeScalars.append(scalar)
                continue
            }

            guard let introducer = scalars.next() else {
                break
            }
            if introducer.value != 0x5b {
                continue
            }

            while let sequenceScalar = scalars.next() {
                if sequenceScalar.value >= 0x40 && sequenceScalar.value <= 0x7e {
                    break
                }
            }
        }

        return output
    }
}
