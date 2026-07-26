import Foundation

/// The machine directory exposed as `OrlixOS.shared` through the module namespace.
public let shared = OrlixMachineDirectory()

public final class OrlixMachineDirectory: @unchecked Sendable {
    public let machines: [OrlixMachine]
    private let sessionLock = NSLock()
    private var kernelSessions: [String: OrlixKernelSession] = [:]
    private var terminalSessionsByMachine: [String: [OrlixTerminalSession]] = [:]

  public var defaultMachine: OrlixMachine {
    machines[0]
  }

    fileprivate init() {
        OrlixPrivatePackageProviders.preconditionLinked()
        machines = [OrlixMachine(id: "default", name: "Default Machine")]
    }

  public func machine(id: String) -> OrlixMachine? {
    machines.first { $0.id == id }
  }

  public func containers(for machine: OrlixMachine) -> Containers {
    machine.containers
  }

  public func processes(
    for container: Containers.Container
  ) -> [Containers.Process] {
    container.processes
  }

    public func terminals(for machine: OrlixMachine) -> [OrlixTerminalSession] {
        guard machines.contains(where: { $0 === machine }) else { return [] }
        sessionLock.lock()
        defer { sessionLock.unlock() }
        return terminalSessionsByMachine[machine.id] ?? []
    }

    public func openTerminal(
        for machine: OrlixMachine,
        bootConfig: OrlixBootConfig
    ) -> OrlixTerminalSession? {
        guard machines.contains(where: { $0 === machine }) else { return nil }
        let terminal = OrlixTerminalSession()
        let kernelSession = OrlixKernelSession(
            bootConfig: bootConfig,
            terminal: terminal
        )
        terminal.bind(kernelSession: kernelSession)

        sessionLock.lock()
        kernelSessions[terminal.id] = kernelSession
        terminalSessionsByMachine[machine.id, default: []].append(terminal)
        sessionLock.unlock()
        return terminal
    }

    public func closeTerminal(
        _ terminal: OrlixTerminalSession,
        for machine: OrlixMachine
    ) {
        guard machines.contains(where: { $0 === machine }) else { return }
        sessionLock.lock()
        kernelSessions.removeValue(forKey: terminal.id)
        terminalSessionsByMachine[machine.id]?.removeAll { $0 === terminal }
        sessionLock.unlock()
    }
}

/// A persistent namespaced Linux system hosted by the process-wide kernel.
public final class OrlixMachine: @unchecked Sendable, Identifiable {
  public let id: String
  public let name: String
  public let containers: Containers

  fileprivate init(id: String, name: String) {
    self.id = id
    self.name = name
    containers = Containers(machineID: id)
  }
}

/// Container resources belonging to exactly one machine.
public final class Containers: @unchecked Sendable {
  public enum BackendState: Equatable, Sendable {
    case unavailable
  }

  public let machineID: String
  public let backendState: BackendState = .unavailable

  fileprivate init(machineID: String) {
    self.machineID = machineID
  }

  public var all: [Container] {
    []
  }

  public func container(id: String) -> Container? {
    nil
  }

  public struct Container: Equatable, Identifiable, Sendable {
    public enum State: Equatable, Sendable {
      case created
      case running
      case stopped(exitCode: Int32?)
      case failed
    }

    public let id: String
    public let imageReference: String
    public let state: State
    public let processes: [Process]

    init(
      id: String,
      imageReference: String,
      state: State,
      processes: [Process]
    ) {
      self.id = id
      self.imageReference = imageReference
      self.state = state
      self.processes = processes
    }
  }

  public struct Process: Equatable, Identifiable, Sendable {
    public enum State: Equatable, Sendable {
      case created
      case running(pid: Int32)
      case exited(status: Int32)
      case signaled(signal: Int32)
    }

    public let id: String
    public let arguments: [String]
    public let state: State
    public let terminalSessionID: String?

    init(
      id: String,
      arguments: [String],
      state: State,
      terminalSessionID: String?
    ) {
      self.id = id
      self.arguments = arguments
      self.state = state
      self.terminalSessionID = terminalSessionID
    }
  }
}
