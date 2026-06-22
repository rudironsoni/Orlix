import XCTest
@testable import OrlixTestRunner

final class OrlixKernelUpstreamTests: XCTestCase {
    func testProcessLifecycleProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelProcessLifecycle)

        XCTAssertTrue(output.contains("process_lifecycle_probe"))
        XCTAssertTrue(output.contains("ORLIX-PROCESS-LIFECYCLE-PROBE"))
        XCTAssertTrue(output.contains("forked child exit status is reported by waitpid"))
        XCTAssertTrue(output.contains("forked child exec status is reported by waitpid"))
        XCTAssertTrue(output.contains("forked exec observes Linux argv env and cwd"))
        XCTAssertTrue(output.contains("waited child is reaped with ECHILD on second wait"))
        XCTAssertTrue(output.contains("child reads Linux procfs status before exit"))
        XCTAssertTrue(output.contains("signal-terminated child reports Linux wait status"))
        XCTAssertTrue(output.contains("process has Linux PID allocated by the kernel"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testMountNamespaceProbeVerifiesMountinfoThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelMountNamespace)

        XCTAssertTrue(output.contains("mount_namespace_probe"))
        XCTAssertTrue(output.contains("mount namespace child verified mountinfo"))
        XCTAssertTrue(output.contains("child tmpfs mount is hidden from parent"))
        XCTAssertFalse(output.contains("clone_thread_probe"))
    }

    func testEnvironmentEntryProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelEnvironmentEntry)

        XCTAssertTrue(output.contains("environment_entry_probe"))
        XCTAssertTrue(output.contains("environment entry parent marker created"))
        XCTAssertTrue(output.contains("environment entry child started"))
        XCTAssertTrue(output.contains("environment entry child root entered"))
        XCTAssertTrue(output.contains("environment entry child exited cleanly"))
        XCTAssertTrue(output.contains("environment entry root is hidden from parent"))
        XCTAssertTrue(output.contains("environment entry parent marker cleaned"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testInitExecProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelInitExec)

        XCTAssertTrue(output.contains("init_exec_probe"))
        XCTAssertTrue(output.contains("ORLIX-INIT-EXEC-PROBE"))
        XCTAssertTrue(output.contains("fork creates a child task"))
        XCTAssertTrue(output.contains("waitpid returns the forked child"))
        XCTAssertTrue(output.contains("waitpid observes the child exit status"))
        XCTAssertTrue(output.contains("mmap syscall returns writable memory"))
        XCTAssertTrue(
            output.contains(
                "writable anonymous mmap stays inside hosted user window"
            )
        )
        XCTAssertTrue(output.contains("forked child execs current image and exits"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testFDExecProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelFDExec)

        XCTAssertTrue(output.contains("fd_exec_probe"))
        XCTAssertTrue(output.contains("ORLIX-FD-EXEC-PROBE"))
        XCTAssertTrue(output.contains("pipe creates descriptor pairs"))
        XCTAssertTrue(
            output.contains(
                "fcntl reports descriptor without close-on-exec"
            )
        )
        XCTAssertTrue(
            output.contains("fcntl marks selected descriptor close-on-exec")
        )
        XCTAssertTrue(output.contains("ORLIX-FD-EXEC-CHILD"))
        XCTAssertTrue(output.contains("ORLIX-FD-INHERITED-READ-OK"))
        XCTAssertTrue(output.contains("ORLIX-FD-CLOEXEC-EBADF-OK"))
        XCTAssertTrue(
            output.contains("exec preserves non-close-on-exec descriptor")
        )
        XCTAssertTrue(output.contains("exec closes close-on-exec descriptor"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testFDAliasProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelFDAlias)

        XCTAssertTrue(output.contains("fd_alias_probe"))
        XCTAssertTrue(output.contains("ORLIX-FD-ALIAS-PROBE"))
        XCTAssertTrue(output.contains("/dev/fd is a directory"))
        XCTAssertTrue(
            output.contains("/dev/fd opens the referenced descriptor path")
        )
        XCTAssertTrue(output.contains("/dev/stdin aliases fd 0"))
        XCTAssertTrue(output.contains("/dev/stdout aliases fd 1"))
        XCTAssertTrue(output.contains("/dev/stderr aliases fd 2"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testSignalWaitProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelSignalWait)

        XCTAssertTrue(output.contains("signal_wait_probe"))
        XCTAssertTrue(output.contains("ORLIX-SIGNAL-WAIT-PROBE"))
        XCTAssertTrue(
            output.contains("signal handler runs for delivered signal")
        )
        XCTAssertTrue(output.contains("blocked signal remains pending"))
        XCTAssertTrue(
            output.contains("unblocked pending signal runs handler")
        )
        XCTAssertTrue(
            output.contains("waitpid observes signal termination status")
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testPipePollProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelPipePoll)

        XCTAssertTrue(output.contains("pipe_poll_probe"))
        XCTAssertTrue(output.contains("ORLIX-PIPE-POLL-PROBE"))
        XCTAssertTrue(output.contains("pipe creates nonblocking read descriptor"))
        XCTAssertTrue(
            output.contains("empty nonblocking pipe read returns EAGAIN")
        )
        XCTAssertTrue(output.contains("empty pipe read poll times out"))
        XCTAssertTrue(output.contains("pipe write end polls writable"))
        XCTAssertTrue(
            output.contains("pipe read end polls readable after write")
        )
        XCTAssertTrue(output.contains("pipe read returns written payload"))
        XCTAssertTrue(
            output.contains("pipe read end polls hangup after writer closes")
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testPipeSelectProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelPipeSelect)

        XCTAssertTrue(output.contains("pipe_select_probe"))
        XCTAssertTrue(output.contains("ORLIX-PIPE-SELECT-PROBE"))
        XCTAssertTrue(
            output.contains(
                "pipe creates nonblocking read descriptor for select"
            )
        )
        XCTAssertTrue(
            output.contains(
                "empty nonblocking pipe read returns EAGAIN before select"
            )
        )
        XCTAssertTrue(output.contains("empty pipe read select times out"))
        XCTAssertTrue(output.contains("pipe write end selects writable"))
        XCTAssertTrue(
            output.contains("pipe read end selects readable after write")
        )
        XCTAssertTrue(output.contains("pipe read returns selected payload"))
        XCTAssertTrue(
            output.contains(
                "pipe read end selects readable after writer closes"
            )
        )
        XCTAssertTrue(
            output.contains("pipe read returns EOF after selected writer close")
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testPipeEpollProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelPipeEpoll)

        XCTAssertTrue(output.contains("pipe_epoll_probe"))
        XCTAssertTrue(output.contains("ORLIX-PIPE-EPOLL-PROBE"))
        XCTAssertTrue(
            output.contains(
                "pipe creates nonblocking read descriptor for epoll"
            )
        )
        XCTAssertTrue(output.contains("epoll_create1 returns epoll descriptor"))
        XCTAssertTrue(
            output.contains(
                "empty nonblocking pipe read returns EAGAIN before epoll"
            )
        )
        XCTAssertTrue(output.contains("epoll_ctl adds pipe read end"))
        XCTAssertTrue(output.contains("empty pipe read epoll times out"))
        XCTAssertTrue(output.contains("pipe write end epolls writable"))
        XCTAssertTrue(
            output.contains("pipe read end epolls readable after write")
        )
        XCTAssertTrue(output.contains("pipe read returns epoll payload"))
        XCTAssertTrue(
            output.contains("pipe read end epolls hangup after writer closes")
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testPseudoFSProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelPseudoFS)

        XCTAssertTrue(output.contains("pseudo_fs_probe"))
        XCTAssertTrue(output.contains("ORLIX-PSEUDO-FS-PROBE"))
        XCTAssertTrue(output.contains("mountinfo exposes procfs at /proc"))
        XCTAssertTrue(output.contains("mountinfo exposes sysfs at /sys"))
        XCTAssertTrue(output.contains("mountinfo exposes devtmpfs at /dev"))
        XCTAssertTrue(output.contains("mountinfo exposes devpts at /dev/pts"))
        XCTAssertTrue(output.contains("mountinfo exposes tmpfs at /tmp"))
        XCTAssertTrue(
            output.contains("proc self status fd and mounts are readable")
        )
        XCTAssertTrue(
            output.contains("core dev nodes are Linux character devices")
        )
        XCTAssertTrue(output.contains("devpts mountpoint is a directory"))
        XCTAssertTrue(
            output.contains("devpts allocates a PTY master through ptmx")
        )
        XCTAssertTrue(
            output.contains("sysfs exposes virtio device directory")
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testCgroupV2ProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelCgroupV2)

        XCTAssertTrue(output.contains("cgroup_v2_probe"))
        XCTAssertTrue(output.contains("ORLIX-CGROUP-V2-PROBE"))
        XCTAssertTrue(
            output.contains("mountinfo exposes cgroup2 at /sys/fs/cgroup")
        )
        XCTAssertTrue(
            output.contains("cgroup v2 controllers file is readable")
        )
        XCTAssertTrue(output.contains("cgroup v2 procs file is readable"))
        XCTAssertTrue(output.contains("proc self cgroup reports unified root"))
        XCTAssertTrue(output.contains("cgroup v2 procs accepts current task"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testCgroupNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelCgroupNamespace)

        XCTAssertTrue(output.contains("cgroup_namespace_probe"))
        XCTAssertTrue(output.contains("cgroup namespace unshare changes /proc/self/ns/cgroup"))
        XCTAssertTrue(output.contains("cgroup namespace keeps /proc/self/cgroup readable"))
    }

    func testCgroupPidsProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelCgroupPids)
        XCTAssertTrue(output.contains("cgroup_pids_probe"))
        XCTAssertTrue(output.contains("cgroup v2 exposes pids controller"))
        XCTAssertTrue(output.contains("cgroup v2 enables pids controller for children"))
        XCTAssertTrue(output.contains("child cgroup exposes pids controller files"))
        XCTAssertTrue(output.contains("pids controller accepts max limit"))
        XCTAssertTrue(output.contains("pids cgroup accepts current task"))
        XCTAssertFalse(output.contains("/orlix/mount_namespace_probe"))
    }

    func testUserNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelUserNamespace)

        XCTAssertTrue(output.contains("user_namespace_probe"))
        XCTAssertTrue(output.contains("user namespace unshare changes /proc/self/ns/user"))
        XCTAssertTrue(output.contains("user namespace exposes readable uid_map and gid_map"))
        XCTAssertTrue(output.contains("user namespace exposes setgroups control"))
    }

    func testOverlayFSProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelOverlayFS)
        XCTAssertTrue(output.contains("overlayfs_probe"))
        XCTAssertTrue(output.contains("overlayfs mounts and reads lower files"))
        XCTAssertTrue(output.contains("overlayfs copy-up preserves lower files"))
        XCTAssertTrue(output.contains("overlayfs unlink hides lower files"))
        XCTAssertFalse(output.contains("/orlix/mount_namespace_probe"))
    }

    func testTimeNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelTimeNamespace)
        XCTAssertTrue(output.contains("time_namespace_probe"))
        XCTAssertTrue(output.contains("time namespace proc entry is readable"))
        XCTAssertTrue(output.contains("time_for_children proc entry is readable"))
        XCTAssertTrue(output.contains("unshare CLONE_NEWTIME succeeds"))
        XCTAssertTrue(output.contains("unshare prepares time namespace for children"))
        XCTAssertTrue(output.contains("forked child enters unshared time namespace"))
        XCTAssertTrue(output.contains("time namespace exposes timens_offsets"))
        XCTAssertFalse(output.contains("/orlix/mount_namespace_probe"))
    }

    func testIPCNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelIPCNamespace)
        XCTAssertTrue(output.contains("ipc_namespace_probe"))
        XCTAssertTrue(output.contains("IPC namespace isolates SysV shared memory keys"))
        XCTAssertTrue(output.contains("IPC namespace isolates SysV message queue keys"))
        XCTAssertTrue(output.contains("IPC namespace isolates POSIX message queue names"))
        XCTAssertFalse(output.contains("/orlix/mount_namespace_probe"))
    }

    func testPathErrnoProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelPathErrno)

        XCTAssertTrue(output.contains("path_errno_probe"))
        XCTAssertTrue(
            output.contains("path errno fixture created through Linux VFS")
        )
        XCTAssertTrue(output.contains("ORLIX-ORACLE-BEGIN path-errno"))
        XCTAssertTrue(
            output.contains(
                #"{"operation":"open","path":"missing","errno":2"#
            )
        )
        XCTAssertTrue(output.contains("missing path returns ENOENT"))
        XCTAssertTrue(
            output.contains(
                #"{"operation":"open","path":"regular/child","errno":20"#
            )
        )
        XCTAssertTrue(output.contains("non-directory child returns ENOTDIR"))
        XCTAssertTrue(
            output.contains(
                #"{"operation":"stat","path":"loop-a","errno":40"#
            )
        )
        XCTAssertTrue(output.contains("symlink loop returns ELOOP"))
        XCTAssertTrue(
            output.contains(
                #"{"operation":"stat","path":"regular/","errno":20"#
            )
        )
        XCTAssertTrue(
            output.contains("trailing slash on regular file returns ENOTDIR")
        )
        XCTAssertTrue(output.contains("ORLIX-ORACLE-END path-errno"))
        XCTAssertTrue(output.contains("path errno fixture cleaned"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testCloneThreadProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelCloneThread)

        XCTAssertTrue(output.contains("clone_thread_probe"))
        XCTAssertTrue(
            output.contains(
                "mlibc-shaped clone thread stack runs through Linux clone"
            )
        )
        XCTAssertTrue(
            output.contains(
                "mlibc-shaped clone TLS and futex join handshake completes"
            )
        )
        XCTAssertTrue(
            output.contains(
                "mlibc-shaped clone stack supports deep alloca faults"
            )
        )
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testBootProfileContractVerifiesVirtioConsoleThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelBootProfile)

        XCTAssertTrue(output.contains("boot_profile_contract"))
        XCTAssertTrue(output.contains("cmdline selects the Orlix virtio console"))
        XCTAssertTrue(output.contains("live consoles include the Orlix virtio console"))
        XCTAssertTrue(
            output.contains(
                "live device tree labels vda as immutable base storage"
            )
        )
        XCTAssertTrue(
            output.contains(
                "live device tree labels vdb as writable state storage"
            )
        )
        XCTAssertTrue(output.contains("Linux exposes the immutable base block device"))
        XCTAssertTrue(output.contains("Linux exposes the writable state block device"))
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testVirtioMMIOContractProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelVirtioMMIOContract)

        XCTAssertTrue(output.contains("virtio_mmio_probe_contract"))
        XCTAssertTrue(output.contains("upstream virtio bus exposes devices"))
        XCTAssertTrue(output.contains("upstream virtio bus exposes the virtio-net device"))
        XCTAssertTrue(
            output.contains("upstream virtio-fs device registers the Orlix host-folder tag")
        )
        XCTAssertTrue(output.contains("orlix-host0"))
    }

    func testVirtioNetDeviceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelVirtioNetDevice)

        XCTAssertTrue(output.contains("virtio_net_device_probe"))
        XCTAssertTrue(output.contains("virtio-net device is present on the upstream virtio bus"))
        XCTAssertTrue(output.contains("virtio-net device owns a Linux netdev"))
        XCTAssertTrue(output.contains("virtio-net netdev is exposed through sysfs"))
        XCTAssertTrue(output.contains("virtio-net netdev reports Ethernet hardware type"))
        XCTAssertTrue(output.contains("virtio-net netdev reports Ethernet address length"))
        XCTAssertTrue(output.contains("rtnetlink enumerates the virtio-net Ethernet link"))
        XCTAssertTrue(output.contains("virtio-net link is distinct from loopback"))
        XCTAssertTrue(output.contains("procfs reports the virtio-net interface"))
    }

    func testRandomDeviceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelRandomDevice)

        XCTAssertTrue(output.contains("random_device_probe"))
        XCTAssertTrue(output.contains("Linux getrandom returns random bytes"))
        XCTAssertTrue(output.contains("Linux /dev/urandom returns random bytes"))
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testVirtioFSMountProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelVirtioFSMount)

        XCTAssertTrue(output.contains("virtio_fs_mount_probe"))
    }

    func testVirtioBlockEnvironmentProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelVirtioBlockEnvironment)

        XCTAssertTrue(output.contains("virtio_blk_environment_probe"))
        XCTAssertTrue(
            output.contains(
                "immutable base root is visible as /dev/vda block device"
            )
        )
        XCTAssertTrue(
            output.contains(
                "writable state root is visible as /dev/vdb block device"
            )
        )
        XCTAssertTrue(output.contains("sysfs marks /dev/vda read-only"))
        XCTAssertTrue(output.contains("sysfs marks /dev/vdb writable"))
        XCTAssertTrue(output.contains("sysfs reports nonzero /dev/vda size"))
        XCTAssertTrue(output.contains("sysfs reports nonzero /dev/vdb size"))
        XCTAssertTrue(
            output.contains(
                "sysfs exposes /dev/vda virtio block identifier"
            )
        )
        XCTAssertTrue(
            output.contains(
                "sysfs exposes /dev/vdb virtio block identifier"
            )
        )
        XCTAssertTrue(
            output.contains(
                "/dev/vda serves sector reads through Linux block layer"
            )
        )
        XCTAssertTrue(
            output.contains(
                "/dev/vdb serves sector reads through Linux block layer"
            )
        )
        XCTAssertTrue(output.contains("/dev/vda rejects sector writes"))
        XCTAssertTrue(output.contains("/dev/vdb accepts sector writes"))
        XCTAssertTrue(output.contains("/dev/vdb flushes after sector writes"))
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testRlimitProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelRlimit)

        XCTAssertTrue(output.contains("rlimit_probe"))
        XCTAssertTrue(output.contains("ok - setrlimit/getrlimit RLIMIT_NOFILE"))
        XCTAssertTrue(output.contains("ok - RLIMIT_NOFILE enforces EMFILE"))
        XCTAssertTrue(output.contains("ok - exec child inherited RLIMIT_NOFILE"))
        XCTAssertTrue(output.contains("ok - RLIMIT_NOFILE survives exec"))
    }

    func testProcessCapabilityProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelProcessCapability)

        XCTAssertTrue(output.contains("process_capability_probe"))
        XCTAssertTrue(output.contains("ORLIX-PROCESS-CAPABILITY-PROBE"))
        XCTAssertTrue(output.contains("capget reads current Linux capability sets"))
        XCTAssertTrue(output.contains("proc self status exposes Linux capability fields"))
        XCTAssertTrue(output.contains("capset accepts current Linux capability sets"))
        XCTAssertTrue(output.contains("capset effective changes are visible in proc status"))
        XCTAssertTrue(output.contains("prctl reads Linux capability bounding set"))
        XCTAssertTrue(output.contains("prctl ambient clear is visible in proc status"))
        XCTAssertFalse(output.contains("# exec /orlix/mount_namespace_probe"))
    }

    func testUmaskProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelUmask)

        XCTAssertTrue(output.contains("umask_probe"))
        XCTAssertTrue(
            output.contains("ok - umask masks file and directory creation modes")
        )
        XCTAssertTrue(output.contains("ok - exec child inherited umask"))
        XCTAssertTrue(output.contains("ok - umask survives exec"))
        XCTAssertTrue(output.contains("ok - child umask changes stay process-local"))
    }

    func testReadonlyRootProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelReadonlyRoot)

        XCTAssertTrue(output.contains("readonly_root_probe"))
        XCTAssertTrue(output.contains("orlix.root.readonly=1"))
    }

    func testHostnameDomainnameProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelHostnameDomainname)

        XCTAssertTrue(output.contains("hostname_domainname_probe"))
        XCTAssertTrue(output.contains("orlix.hostname=oci-host"))
        XCTAssertTrue(output.contains("orlix.domainname=oci.example"))
    }

    func testNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernelNamespace)

        XCTAssertTrue(output.contains("namespace_probe"))
        XCTAssertTrue(output.contains("UTS namespace supports private hostname"))
        XCTAssertTrue(output.contains("UTS namespace supports private domainname"))
        XCTAssertTrue(output.contains("UTS namespace child names do not leak to parent"))
    }

    func testNetworkNamespaceProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelNetworkNamespace)

        XCTAssertTrue(output.contains("network_namespace_probe"))
        XCTAssertTrue(output.contains("procfs exposes network state"))
        XCTAssertTrue(output.contains("rtnetlink sockets open in the current network namespace"))
        XCTAssertTrue(output.contains("RTM_GETLINK reports loopback interface"))
        XCTAssertTrue(output.contains("loopback interface accepts Linux address configuration"))
        XCTAssertTrue(output.contains("RTM_GETADDR reports loopback IPv4 address"))
        XCTAssertTrue(output.contains("RTM_GETROUTE reports loopback IPv4 route"))
        XCTAssertTrue(output.contains("network namespace child enters isolated net namespace"))
        XCTAssertTrue(output.contains("new network namespace keeps procfs network state readable"))
        XCTAssertTrue(output.contains("new network namespace keeps rtnetlink socket local"))
        XCTAssertTrue(output.contains("new network namespace rejects incomplete route with Linux error"))
        XCTAssertTrue(output.contains("loopback TCP accepts local connections"))
        XCTAssertTrue(output.contains("loopback UDP exchanges local datagrams"))
    }

    func testEnvironmentStateWritebackProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(.kernelEnvironmentStateWriteback)

        XCTAssertTrue(output.contains("environment_state_writeback_probe"))
        XCTAssertTrue(output.contains("writable state block mounts as ext4"))
        XCTAssertTrue(output.contains("environment state marker write succeeds"))
        XCTAssertTrue(output.contains("environment state marker sync succeeds"))
        XCTAssertTrue(output.contains("writable state block remounts after sync"))
        XCTAssertTrue(output.contains("environment state marker reread succeeds"))
        XCTAssertTrue(output.contains("immutable base block still rejects writes"))
        XCTAssertTrue(output.contains("writable state block still flushes writes"))
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testEnvironmentStateCrossbootWriteProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(
            .kernelEnvironmentStateCrossbootWrite
        )

        XCTAssertTrue(output.contains("environment_state_crossboot_write_probe"))
        XCTAssertTrue(
            output.contains("cross-boot writable state block mounts as ext4")
        )
        XCTAssertTrue(output.contains("cross-boot state marker write succeeds"))
        XCTAssertTrue(output.contains("cross-boot state marker sync succeeds"))
        XCTAssertTrue(
            output.contains(
                "cross-boot state marker remains readable before boot boundary"
            )
        )
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testEnvironmentStateCrossbootVerifyProbeCompletesThroughOrlixOSTerminalSession()
        throws
    {
        let output = try OrlixUpstreamXCTest.run(
            .kernelEnvironmentStateCrossbootVerify
        )

        XCTAssertTrue(output.contains("environment_state_crossboot_verify_probe"))
        XCTAssertTrue(
            output.contains("cross-boot writable state block remounts as ext4")
        )
        XCTAssertTrue(
            output.contains(
                "cross-boot state marker survives fresh boot boundary"
            )
        )
        XCTAssertTrue(output.contains("cross-boot state marker cleanup succeeds"))
        XCTAssertFalse(output.contains("# exec /orlix/clone_thread_probe"))
    }

    func testKselftestRootfsCompletesThroughOrlixOSTerminalSession() throws {
        let output = try OrlixUpstreamXCTest.run(.kernel)

        XCTAssertTrue(output.contains("environment_entry_probe"))
        XCTAssertTrue(output.contains("environment entry child exited cleanly"))
        XCTAssertTrue(output.contains("mount_namespace_probe"))
        XCTAssertTrue(output.contains("mount namespace child verified mountinfo"))
        XCTAssertTrue(output.contains("child tmpfs mount is hidden from parent"))
        XCTAssertTrue(output.contains("virtio_blk_environment_probe"))
        XCTAssertTrue(output.contains("virtio_mmio_probe_contract"))
        XCTAssertTrue(output.contains("random_device_probe"))
        XCTAssertTrue(output.contains("Linux getrandom returns random bytes"))
        XCTAssertTrue(output.contains("Linux /dev/urandom returns random bytes"))
        XCTAssertTrue(
            output.contains("upstream hwrng device returns virtio-backed entropy")
        )
    }
}
