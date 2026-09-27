"""Product Xcode pin versus allowed local cache identities."""

from __future__ import annotations

import json
import hashlib
import os
import platform
import subprocess
import shutil
import time
from pathlib import Path

PIN_PATH = Path(__file__).with_name("toolchain-pin.json")
OBSERVE_TIMEOUT_SECONDS = 30
OBSERVE_MAX_ATTEMPTS = 2


class PinError(RuntimeError):
    pass


def load_pin(path: Path = PIN_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def product_pin(data: dict | None = None) -> dict:
    return (data or load_pin())["product_pin"]


def allowed_identities(data: dict | None = None) -> tuple[tuple[str, str], ...]:
    payload = data or load_pin()
    return tuple(
        (entry["xcode_version"], entry["xcode_build"]) for entry in payload["allowed_local"]
    )


def namespace_for(version: str, build: str, data: dict | None = None) -> str:
    payload = data or load_pin()
    for entry in payload["allowed_local"]:
        if entry["xcode_version"] == version and entry["xcode_build"] == build:
            return entry["disk_cache_namespace"]
    raise PinError(f"unsupported Xcode identity {version} {build}")


def require_identity(version: str, build: str, disk_cache: str, data: dict | None = None) -> None:
    payload = data or load_pin()
    try:
        expected = namespace_for(version, build, payload)
    except PinError:
        raise PinError(f"unsupported Xcode identity {version} {build}")
    if expected not in disk_cache:
        raise PinError(
            f"disk cache {disk_cache} is not namespaced as {expected}"
        )


def _observe_failure(command: tuple[str, ...], context: dict, reason: str, elapsed: list[float]) -> PinError:
    total = sum(elapsed)
    detail = [f"Xcode {context['xcode']} build {context['build']}"]
    if "--sdk" in command:
        detail.append(f"sdk {command[command.index('--sdk') + 1]}")
    detail.append(f"timeout {OBSERVE_TIMEOUT_SECONDS}s")
    detail.append(f"elapsed {total:.1f}s over {len(elapsed)} attempt(s)")
    return PinError(f"toolchain discovery {reason}: {' '.join(command)} ({', '.join(detail)})")


def capture_manifest(developer_dir: str, bazel: str, output: str, run_id: str | None = None) -> dict:
    developer = str(Path(developer_dir).resolve(strict=True))
    env = {**os.environ, "DEVELOPER_DIR": developer}
    context = {"xcode": "unknown", "build": "unknown"}

    def observe(*command: str) -> str:
        elapsed: list[float] = []
        for _ in range(OBSERVE_MAX_ATTEMPTS):
            start = time.monotonic()
            try:
                return subprocess.run(
                    command, env=env, check=True, capture_output=True, text=True,
                    timeout=OBSERVE_TIMEOUT_SECONDS,
                ).stdout.strip()
            except subprocess.TimeoutExpired as error:
                elapsed.append(time.monotonic() - start)
                if len(elapsed) >= OBSERVE_MAX_ATTEMPTS:
                    # from None: the PinError message already carries command,
                    # SDK, Xcode identity, timeout, and elapsed time.
                    raise _observe_failure(command, context, "timed out", elapsed) from None
            except subprocess.CalledProcessError as error:
                elapsed.append(time.monotonic() - start)
                reason = f"failed (exit {error.returncode})"
                stderr = (error.stderr or "").strip().splitlines()
                if stderr:
                    reason += f": {stderr[-1].strip()[:200]}"
                raise _observe_failure(command, context, reason, elapsed) from None
        raise AssertionError("unreachable: observe attempts exhausted without verdict")

    xcode = observe("/usr/bin/xcodebuild", "-version").splitlines()
    version = xcode[0].removeprefix("Xcode ")
    build = xcode[1].removeprefix("Build version ")
    context.update({"xcode": version, "build": build})
    namespace_for(version, build)
    tools = {}
    for name in ("clang", "ld", "swift", "metal", "bazel"):
        path = Path(bazel if name == "bazel" else observe("/usr/bin/xcrun", "--find", name)).resolve(strict=True)
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        tools[name] = {"path": str(path), "sha256": digest}
    payload = {
        "schema": 1,
        "kind": "observed-toolchain",
        "developer_dir": developer,
        "run_id": run_id,
        "xcode_version": version,
        "xcode_build": build,
        "bazel_version": observe(bazel, "--version"),
        "host_macos": platform.mac_ver()[0],
        "host_arch": platform.machine(),
        "tools": tools,
        "sdks": {
            sdk: {
                "version": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-version"),
                "build": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-build-version"),
                "path": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-path"),
            }
            for sdk in ("iphoneos", "iphonesimulator", "macosx")
        },
    }
    if payload["bazel_version"] != f"bazel {product_pin()['bazel']}":
        raise PinError("observed Bazel does not match the product pin")
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = destination.with_name(f"{destination.name}.tmp-{os.getpid()}")
    staging.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    os.replace(staging, destination)
    return payload


def validate_manifest(path: str, developer_dir: str, run_id: str | None, disk_cache: str) -> dict:
    """Establish that an existing manifest belongs to the current run.

    Rejects missing files, stale run ids, a changed developer directory, and
    any Xcode/Bazel identity outside the accepted pin. Never captures.
    """
    try:
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
    except OSError as error:
        raise PinError(f"toolchain manifest is missing: {path}") from error
    except ValueError as error:
        raise PinError(f"toolchain manifest is not valid JSON: {path}") from error
    developer = str(Path(developer_dir).resolve(strict=True))
    if payload.get("developer_dir") != developer:
        raise PinError(
            f"stale toolchain manifest: {payload.get('developer_dir')} is not the selected {developer}"
        )
    if payload.get("run_id") != run_id:
        raise PinError(
            f"stale toolchain manifest: run {payload.get('run_id')!r} is not the current run {run_id!r}"
        )
    try:
        require_identity(payload["xcode_version"], payload["xcode_build"], disk_cache)
    except (PinError, KeyError) as error:
        raise PinError(f"stale toolchain manifest: unsupported identity in {path}") from error
    if payload.get("bazel_version") != f"bazel {product_pin()['bazel']}":
        raise PinError(f"stale toolchain manifest: Bazel {payload.get('bazel_version')} is not the pinned Bazel")
    return payload


def ensure_current_manifest(
    developer_dir: str, bazel: str, output: str, run_id: str | None, disk_cache: str
) -> dict:
    """Reuse the current run's manifest, or capture it once when absent or stale.

    Capture itself re-observes the toolchain and enforces the accepted
    identity, so a stale manifest is replaced, never reused.
    """
    try:
        return validate_manifest(output, developer_dir, run_id, disk_cache)
    except PinError:
        return capture_manifest(developer_dir, bazel, output, run_id)


def capture_kernel_manifest(developer_dir: str, output: str) -> dict:
    developer = Path(developer_dir).resolve(strict=True)
    environment = {**os.environ, "DEVELOPER_DIR": str(developer)}
    sdk = Path(subprocess.check_output(
        ["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"],
        env=environment, text=True,
    ).strip()).resolve(strict=True)
    xcode_tools = developer / "Toolchains/XcodeDefault.xctoolchain/usr"
    tools = [
        Path("/opt/homebrew/opt/llvm/bin") / name
        for name in ("clang", "llvm-ar", "llvm-nm")
    ] + [
        Path("/opt/homebrew/opt/lld/bin/ld.lld"),
        Path("/opt/homebrew/bin/gmake"),
        Path("/opt/homebrew/opt/gnu-sed/libexec/gnubin/sed"),
        Path("/opt/homebrew/opt/coreutils/libexec/gnubin/readlink"),
        xcode_tools / "bin/clang", xcode_tools / "bin/ld",
    ]
    search_path = "/opt/homebrew/opt/gnu-sed/libexec/gnubin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/findutils/libexec/gnubin:/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin"
    names = ("perl", "awk", "stat", "gzip", "git", "sort", "tr", "find", "strings", "nm", "otool")
    tools += [Path(shutil.which(name, path=search_path)) for name in names]
    tools += [Path(path) for path in ("/usr/bin/python3", "/usr/bin/patch", "/usr/bin/rsync", "/opt/homebrew/bin/ccache", "/bin/bash", "/bin/cp", "/usr/bin/shasum")]
    for name in ("python3", "nm", "otool"):
        tools.append(Path(subprocess.check_output(["/usr/bin/xcrun", "--find", name], env=environment, text=True).strip()))
    tools.append(sdk / "SDKSettings.json")
    trees = [sdk / "usr/include", sdk / "usr/lib", Path("/opt/homebrew/opt/llvm/lib/clang"), xcode_tools / "lib/clang"]
    tools += sorted(Path("/opt/homebrew/opt/llvm/lib").glob("*.dylib"))
    tools += sorted((xcode_tools / "lib").glob("*.dylib"))
    files = {}
    for path in tools:
        files[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    tree_hashes = {}
    for tree in trees:
        digest = hashlib.sha256()
        for path in sorted(tree.rglob("*")):
            if path.is_symlink():
                data = str(path.readlink()).encode()
            elif path.is_file():
                data = path.read_bytes()
            else:
                continue
            digest.update(path.relative_to(tree).as_posix().encode() + b"\0")
            digest.update(hashlib.sha256(data).digest())
        tree_hashes[str(tree)] = digest.hexdigest()
    payload = {
        "schema": 1, "files": files, "trees": tree_hashes,
        "macos": platform.mac_ver()[0], "host_arch": platform.machine(),
    }
    Path(output).write_text(json.dumps(payload, sort_keys=True) + "\n")
    compiler = {
        "schema": 1, "macos": payload["macos"], "host_arch": payload["host_arch"],
        "files": {name: value for name, value in files.items() if name.endswith("/clang") or name.endswith(".dylib")},
        "trees": {name: value for name, value in tree_hashes.items() if name.endswith("/lib/clang")},
    }
    Path(output).with_name("compiler-identity.json").write_text(json.dumps(compiler, sort_keys=True) + "\n")
    runtime_paths = {
        xcode_tools / "bin/clang",
        Path("/opt/homebrew/opt/llvm/bin/llvm-ar"),
    }
    pending = list(runtime_paths)
    while pending:
        binary = pending.pop()
        libraries = subprocess.check_output(["/usr/bin/otool", "-arch", platform.machine(), "-L", str(binary)], text=True)
        for line in libraries.splitlines()[1:]:
            name = line.strip().split(" (", 1)[0]
            if name.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if name.startswith("@rpath/"):
                path = Path("/opt/homebrew/opt/llvm/lib") / name.removeprefix("@rpath/")
            elif name.startswith("/opt/homebrew/"):
                path = Path(name)
            else:
                raise PinError(f"unresolved compiler runtime dependency: {name}")
            if path not in runtime_paths:
                runtime_paths.add(path)
                pending.append(path)
    runtime = {
        "schema": 1, "macos": payload["macos"], "host_arch": payload["host_arch"],
        "files": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(runtime_paths)},
        "trees": {str(xcode_tools / "lib/clang"): tree_hashes[str(xcode_tools / "lib/clang")]},
    }
    Path(output).with_name("compiler-runtime-identity.json").write_text(json.dumps(runtime, sort_keys=True) + "\n")
    tools += sorted(runtime_paths)
    mlibc = capture_guest_manifest(developer, sdk, runtime, tree_hashes, "meson")
    tools += [Path(path) for path in mlibc["files"]]
    trees += [Path(path) for path in mlibc["trees"]]
    Path(output).with_name("mlibc-identity.json").write_text(json.dumps(mlibc, sort_keys=True) + "\n")
    coreutils = capture_guest_manifest(developer, sdk, runtime, tree_hashes, "autotools")
    tools += [Path(path) for path in coreutils["files"]]
    trees += [Path(path) for path in coreutils["trees"]]
    Path(output).with_name("coreutils-identity.json").write_text(json.dumps(coreutils, sort_keys=True) + "\n")
    autotools_tools = tuple(Path("/opt/homebrew/opt/coreutils/libexec/gnubin") / name for name in ("ls", "dirname", "mktemp", "sleep", "stat"))
    autotools = capture_guest_manifest(developer, sdk, runtime, tree_hashes, "autotools", autotools_tools)
    tools += [Path(path) for path in autotools["files"]]
    trees += [Path(path) for path in autotools["trees"]]
    Path(output).with_name("autotools-identity.json").write_text(json.dumps(autotools, sort_keys=True) + "\n")
    bootstrap = capture_guest_manifest(developer, sdk, runtime, tree_hashes, "autotools-bootstrap", autotools_tools)
    tools += [Path(path) for path in bootstrap["files"]]
    trees += [Path(path) for path in bootstrap["trees"]]
    Path(output).with_name("autotools-bootstrap-identity.json").write_text(json.dumps(bootstrap, sort_keys=True) + "\n")
    rootfs_tools = {
        Path(path) for path in (
            "/bin/bash", "/bin/chmod", "/bin/cp", "/bin/dd", "/bin/ln", "/bin/mkdir", "/bin/rm",
            "/usr/bin/awk", "/usr/bin/command", "/usr/bin/find", "/usr/bin/grep", "/usr/bin/gzip",
            "/usr/bin/mktemp", "/usr/bin/printf", "/usr/bin/shasum", "/usr/bin/sort", "/usr/bin/xargs",
            "/opt/homebrew/opt/e2fsprogs/sbin/debugfs", "/opt/homebrew/opt/e2fsprogs/sbin/mke2fs",
        )
    }
    pending = [path for path in rootfs_tools if str(path).startswith("/opt/homebrew/")]
    while pending:
        binary = pending.pop()
        libraries = subprocess.check_output(["/usr/bin/otool", "-arch", platform.machine(), "-L", str(binary)], text=True)
        for line in libraries.splitlines()[1:]:
            name = line.strip().split(" (", 1)[0]
            if name.startswith(("/usr/lib/", "/System/Library/")):
                continue
            path = Path(name)
            if path not in rootfs_tools:
                rootfs_tools.add(path)
                pending.append(path)
    rootfs_tools.add(Path("/opt/homebrew/etc/mke2fs.conf"))
    rootfs = {
        "schema": 1,
        "macos": payload["macos"],
        "host_arch": payload["host_arch"],
        "files": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(rootfs_tools)},
    }
    Path(output).with_name("rootfs-identity.json").write_text(json.dumps(rootfs, sort_keys=True) + "\n")
    tools += sorted(rootfs_tools)
    bash_tools = (Path("/opt/homebrew/opt/llvm/bin/llvm-objdump"),) + tuple(
        Path("/opt/homebrew/opt/coreutils/libexec/gnubin") / name for name in ("ls", "mktemp", "sleep")
    )
    bash = capture_guest_manifest(developer, sdk, runtime, tree_hashes, "autoconf", bash_tools)
    tools += [Path(path) for path in bash["files"]]
    trees += [Path(path) for path in bash["trees"]]
    Path(output).with_name("bash-identity.json").write_text(json.dumps(bash, sort_keys=True) + "\n")
    compiler = {**runtime, "files": {name: digest for name, digest in runtime["files"].items() if name.endswith("/clang")}}
    Path(output).with_name("guest-compiler-identity.json").write_text(json.dumps(compiler, sort_keys=True) + "\n")
    candidates = [str(Path(directory) / name) for directory in search_path.split(":") for name in names]
    return {"files": sorted(set([str(path) for path in tools] + candidates)), "trees": [str(path) for path in trees]}


def capture_guest_manifest(developer: Path, sdk: Path, runtime: dict, tree_hashes: dict, engine: str, extra_tools: tuple[Path, ...] = ()) -> dict:
    scripts = set()
    engine_tools = set(extra_tools)
    modules = []
    if engine == "meson":
        meson = Path("/opt/homebrew/bin/meson")
        interpreter = Path(meson.read_text().splitlines()[0].removeprefix("#!"))
        modules = json.loads(subprocess.check_output([
            str(interpreter), "-I", "-B", "-c",
            "import json,mesonbuild,sysconfig; print(json.dumps([list(mesonbuild.__path__)[0],sysconfig.get_path('stdlib')]))",
        ], text=True))
        scripts.add(meson)
        engine_tools |= {interpreter, Path("/opt/homebrew/bin/ninja"), developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"}
    elif engine in ("autotools", "autoconf", "autotools-bootstrap"):
        scripts.update(Path("/opt/homebrew/bin") / name for name in ("autoconf", "autoheader", "autom4te"))
        modules.append("/opt/homebrew/opt/autoconf/share/autoconf")
        if engine in ("autotools", "autotools-bootstrap"):
            scripts.update(Path("/opt/homebrew/bin") / name for name in ("autoreconf", "automake", "aclocal", "autopoint"))
            version = subprocess.check_output(["/opt/homebrew/bin/automake", "--version"], text=True).splitlines()[0].split()[-1]
            api_version = ".".join(version.split(".")[:2])
            scripts.update(Path("/opt/homebrew/bin") / (name + "-" + api_version) for name in ("aclocal", "automake"))
            for package, patterns in {
                "automake": ("automake-*", "aclocal-*"),
                "gettext": ("gettext", "aclocal"),
            }.items():
                for pattern in patterns:
                    modules.extend(str(path) for path in (Path("/opt/homebrew/opt") / package / "share").glob(pattern))
        if engine == "autotools-bootstrap":
            scripts.update((Path("/opt/homebrew/bin/glibtoolize"), Path("/bin/sh")))
            modules.extend(("/opt/homebrew/opt/libtool/share/libtool", "/opt/homebrew/opt/libtool/share/aclocal", "/opt/homebrew/opt/pkgconf/share/aclocal"))
        engine_tools.update(Path(path) for path in ("/opt/homebrew/bin/gmake", "/opt/homebrew/opt/m4/bin/m4", "/opt/homebrew/bin/pkgconf", "/opt/homebrew/opt/bison/bin/bison", "/opt/homebrew/opt/llvm/bin/llvm-ranlib"))
        engine_tools.update(Path("/opt/homebrew/bin") / name for name in ("ggrep", "gsed", "gawk", "gtar"))
        scripts.update(Path("/usr/bin") / name for name in ("install", "file", "grep", "head", "uname", "basename", "wc", "touch"))
        scripts.update({Path("/bin/chmod"), Path("/bin/expr")})
        engine_tools.update(Path("/opt/homebrew/opt/coreutils/libexec/gnubin") / name for name in ("env", "sort", "tr", "cat", "cp", "mv", "mkdir", "rm", "install", "head", "uname", "expr", "basename", "wc", "touch", "chmod", "printf", "readlink", "ln"))
    else:
        raise PinError(f"unknown guest build engine: {engine}")
    action_python, action_stdlib = json.loads(subprocess.check_output([
        "/usr/bin/python3", "-B", "-c",
        "import json,sys,sysconfig; print(json.dumps([sys.executable,sysconfig.get_path('stdlib')]))",
    ], env={**os.environ, "DEVELOPER_DIR": str(developer)}, text=True))
    modules.append(action_stdlib)
    paths = {
        Path(name) for name in runtime["files"]
    } | {
        Path(action_python),
        developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/ld",
        Path("/opt/homebrew/opt/lld/bin/ld.lld"),
        Path("/opt/homebrew/opt/llvm/bin/llvm-strip"),
        Path("/opt/homebrew/bin/ccache"),
    } | engine_tools
    for module in modules:
        paths.update((Path(module) / "lib-dynload").glob("*.so"))
    pending = list(paths)
    while pending:
        binary = pending.pop()
        load_commands = subprocess.check_output(["/usr/bin/otool", "-arch", platform.machine(), "-l", str(binary)], text=True).splitlines()
        rpaths = []
        library_ids = set()
        for i, line in enumerate(load_commands):
            if line.strip() == "cmd LC_ID_DYLIB":
                library_ids.add(load_commands[i + 2].strip().removeprefix("name ").split(" (offset", 1)[0])
            if line.strip() == "cmd LC_RPATH":
                name = load_commands[i + 2].strip().removeprefix("path ").split(" (offset", 1)[0]
                rpaths.append(Path(name.replace("@loader_path", str(binary.resolve().parent)).replace("@executable_path", str(binary.resolve().parent))))
        libraries = subprocess.check_output(["/usr/bin/otool", "-arch", platform.machine(), "-L", str(binary)], text=True)
        for line in libraries.splitlines()[1:]:
            name = line.strip().split(" (", 1)[0]
            if name in library_ids or name.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if name.startswith("@rpath/"):
                candidates = [path / name.removeprefix("@rpath/") for path in rpaths]
                path = next((path for path in candidates if path.is_file()), None)
                if path is None:
                    raise PinError(f"unresolved guest tool dependency: {name} from {binary}")
            else:
                path = Path(name.replace("@loader_path", str(binary.resolve().parent)).replace("@executable_path", str(binary.resolve().parent)))
            if not path.is_absolute():
                raise PinError(f"unresolved guest tool dependency: {name}")
            if path not in paths:
                paths.add(path)
                pending.append(path)
    paths |= scripts | {Path("/usr/bin/python3"), Path("/usr/bin/patch"), Path("/bin/bash"),
              Path("/bin/cp"), Path("/usr/bin/nm"), sdk / "SDKSettings.json"}
    paths.update(Path("/usr/bin") / name for name in (
        "find", "xargs", "awk", "sort", "tr", "shasum", "xcodebuild", "xcrun",
        "sed", "cmp", "printf", "mktemp", "dirname", "command", "perl", "env",
    ))
    paths.update(Path("/bin") / name for name in ("cat", "mv", "mkdir", "rm"))
    files = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}
    trees = dict(runtime["trees"])
    trees.update({str(path): tree_hashes[str(path)] for path in (sdk / "usr/include", sdk / "usr/lib")})
    for name in modules:
        tree = Path(name)
        digest = hashlib.sha256()
        for path in sorted(tree.rglob("*")):
            relative = path.relative_to(tree)
            if "__pycache__" in relative.parts or "site-packages" in relative.parts or not path.is_file():
                continue
            digest.update(relative.as_posix().encode() + b"\0" + hashlib.sha256(path.read_bytes()).digest())
        trees[str(tree)] = digest.hexdigest()
    return {"schema": 1, "macos": runtime["macos"], "host_arch": runtime["host_arch"], "files": files, "trees": trees}


def macho_linked_libraries(binary: Path) -> list[Path]:
    """Non-system Mach-O dependencies of one executed host tool.

    A text script or a non-Darwin host has no closure. System libraries stay
    outside the action; Homebrew and toolchain libraries are inputs.
    """
    if platform.system() != "Darwin" or not binary.is_file():
        return []
    try:
        load_commands = subprocess.check_output(
            ["/usr/bin/otool", "-arch", platform.machine(), "-l", str(binary)],
            text=True, stderr=subprocess.DEVNULL,
        ).splitlines()
        libraries = subprocess.check_output(
            ["/usr/bin/otool", "-arch", platform.machine(), "-L", str(binary)],
            text=True, stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        return []
    rpaths = []
    library_ids = set()
    for index, line in enumerate(load_commands):
        stripped = line.strip()
        if stripped == "cmd LC_ID_DYLIB" and index + 2 < len(load_commands):
            library_ids.add(load_commands[index + 2].strip().removeprefix("name ").split(" (offset", 1)[0])
        if stripped == "cmd LC_RPATH" and index + 2 < len(load_commands):
            name = load_commands[index + 2].strip().removeprefix("path ").split(" (offset", 1)[0]
            resolved = name.replace("@loader_path", str(binary.resolve().parent)).replace(
                "@executable_path", str(binary.resolve().parent)
            )
            rpaths.append(Path(resolved))
    found = []
    for line in libraries.splitlines()[1:]:
        name = line.strip().split(" (", 1)[0]
        if not name or name in library_ids or name.startswith(("/usr/lib/", "/System/Library/")):
            continue
        if name.startswith("@rpath/"):
            candidates = [path / name.removeprefix("@rpath/") for path in rpaths]
            path = next((candidate for candidate in candidates if candidate.is_file()), None)
            if path is None:
                raise PinError(f"unresolved tool dependency: {name} from {binary}")
        else:
            path = Path(name.replace("@loader_path", str(binary.resolve().parent)).replace(
                "@executable_path", str(binary.resolve().parent)
            ))
        if not path.is_file():
            raise PinError(f"missing tool dependency: {path} from {binary}")
        found.append(path)
    return found


def _copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink() or destination.exists():
        destination.unlink()
    shutil.copy2(source, destination, follow_symlinks=True)
    if os.access(source, os.X_OK):
        destination.chmod(destination.stat().st_mode | 0o755)


def _copy_tree(source: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)

    def ignore(_directory: str, names: list[str]) -> list[str]:
        return [
            name for name in names
            if name in {"__pycache__", "site-packages"} or name.endswith(".pyc")
        ]

    shutil.copytree(source, destination, symlinks=False, ignore=ignore)


def stage_binary(source: Path, bin_dir: Path, lib_dir: Path, seen: set[Path]) -> Path:
    """Copy one executable and its non-system libraries as regular files."""
    if not source.is_file():
        raise PinError(f"executed tool is missing: {source}")
    real = source.resolve()
    destination = bin_dir / source.name
    if real not in seen:
        seen.add(real)
        _copy_file(real, destination)
        pending = [real]
        while pending:
            current = pending.pop()
            for library in macho_linked_libraries(current):
                library_real = library.resolve()
                if library_real in seen:
                    continue
                seen.add(library_real)
                _copy_file(library_real, lib_dir / library_real.name)
                pending.append(library_real)
    elif not destination.exists():
        _copy_file(real, destination)
    return destination


def stage_executed_tools(developer_dir: str, destination: str) -> None:
    """Copy the host tools foreign actions execute into the toolchain repo.

    Identity JSON hashes these bytes and is not the executed file. Actions run
    the copies so the action key contains the tool content.
    """
    developer = Path(developer_dir).resolve(strict=True)
    dest = Path(destination)
    bin_dir = dest / "bin"
    lib_dir = dest / "lib"
    seen: set[Path] = set()
    xcode_bin = developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin"
    for binary in (
        xcode_bin / "clang",
        xcode_bin / "clang++",
        Path("/opt/homebrew/opt/llvm/bin/llvm-ar"),
        Path("/opt/homebrew/opt/llvm/bin/llvm-strip"),
        Path("/opt/homebrew/opt/llvm/bin/llvm-nm"),
        Path("/opt/homebrew/opt/lld/bin/ld.lld"),
        Path("/opt/homebrew/bin/ninja"),
        Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs"),
        Path("/opt/homebrew/opt/e2fsprogs/sbin/debugfs"),
    ):
        stage_binary(binary, bin_dir, lib_dir, seen)
    _copy_file(Path("/opt/homebrew/etc/mke2fs.conf"), dest / "mke2fs.conf")
    resource = developer / "Toolchains/XcodeDefault.xctoolchain/usr/lib/clang"
    if not resource.is_dir():
        raise PinError(f"clang resource directory is missing: {resource}")
    _copy_tree(resource, lib_dir / "clang")
    meson = Path("/opt/homebrew/bin/meson")
    if not meson.is_file():
        raise PinError(f"executed tool is missing: {meson}")
    _copy_file(meson, bin_dir / "meson")
    shebang = meson.read_text(encoding="utf-8").splitlines()[0]
    if not shebang.startswith("#!"):
        raise PinError(f"meson has no interpreter: {meson}")
    interpreter = Path(shebang[2:].strip().split()[0])
    stage_binary(interpreter, bin_dir, lib_dir, seen)
    _copy_file(interpreter, bin_dir / "meson-python")
    info = json.loads(subprocess.check_output(
        [
            str(interpreter), "-B", "-c",
            "import json,mesonbuild,sysconfig; print(json.dumps({"
            "'stdlib': sysconfig.get_path('stdlib'),"
            "'version': sysconfig.get_python_version(),"
            "'meson': list(mesonbuild.__path__)[0]}))",
        ],
        text=True,
    ))
    _copy_tree(Path(info["meson"]), dest / "py" / "mesonbuild")
    _copy_tree(Path(info["stdlib"]), dest / "python-home" / "lib" / ("python" + info["version"]))
    _require_supported_xcode(developer_dir)
    stage_guest_clang(dest / "llvm")
    stage_macos_sdk(developer_dir, dest / "sdk")


def stage_guest_clang(destination: Path) -> None:
    """Copy Homebrew LLVM clang beside its own resource directory.

    Apple clang rejects ``-fuse-ld=ld.lld``. The guest compiler is this LLVM
    clang, staged under its own prefix so it does not replace the Apple clang
    the native build uses with the macOS SDK slice.
    """
    bin_dir = destination / "bin"
    lib_dir = destination / "lib"
    seen: set[Path] = set()
    for binary in (
        Path("/opt/homebrew/opt/llvm/bin/clang"),
        Path("/opt/homebrew/opt/llvm/bin/clang++"),
    ):
        stage_binary(binary, bin_dir, lib_dir, seen)
    resource = Path("/opt/homebrew/opt/llvm/lib/clang")
    if not resource.is_dir():
        raise PinError(f"guest clang resource directory is missing: {resource}")
    _copy_tree(resource, lib_dir / "clang")


def _require_supported_xcode(developer_dir: str) -> None:
    """Reject an unpinned Xcode while staging, not inside the cached action."""
    environment = {**os.environ, "DEVELOPER_DIR": developer_dir}
    version_text = subprocess.check_output(["/usr/bin/xcodebuild", "-version"], env=environment, text=True)
    lines = [line.strip() for line in version_text.splitlines() if line.strip()]
    if len(lines) < 2:
        raise PinError(f"xcodebuild did not report an identity: {version_text!r}")
    version = lines[0].removeprefix("Xcode ").strip()
    build = lines[1].removeprefix("Build version ").strip()
    namespace_for(version, build)


def _resolved_inside(path: Path, root: Path) -> Path | None:
    """Resolve one path when it stays inside root and does not symlink-cycle."""
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError):
        return None
    return resolved


def copy_sdk_slice(sdk: Path, destination: Path) -> None:
    """Copy the host-compile slice of a macOS SDK as regular files.

    The native compiler needs SDKSettings.json, usr/include, and usr/lib.
    Framework trees such as Ruby.framework contain directory symlink cycles, so
    they are not copied and links that leave this slice or cycle are skipped.
    """
    if not (sdk / "SDKSettings.json").is_file():
        raise PinError(f"macOS SDK is missing SDKSettings.json: {sdk}")
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    _copy_file(sdk / "SDKSettings.json", destination / "SDKSettings.json")
    for relative in ("usr/include", "usr/lib"):
        source = sdk / relative
        if not source.is_dir():
            raise PinError(f"macOS SDK slice is missing: {source}")
        root = source.resolve()
        visited: set[Path] = set()

        def copy_directory(current: Path, target: Path) -> None:
            resolved = _resolved_inside(current, root)
            if resolved is None or resolved in visited:
                return
            visited.add(resolved)
            target.mkdir(parents=True, exist_ok=True)
            for child in current.iterdir():
                child_resolved = _resolved_inside(child, root)
                if child_resolved is None:
                    continue
                if child_resolved.is_dir():
                    copy_directory(child, target / child.name)
                elif child_resolved.is_file():
                    _copy_file(child_resolved, target / child.name)

        copy_directory(source, destination / relative)


def stage_macos_sdk(developer_dir: str, destination: Path) -> None:
    """Stage the host SDK slice the sysroot's native compiler compiles against."""
    environment = {**os.environ, "DEVELOPER_DIR": developer_dir}
    sdk = Path(subprocess.check_output(
        ["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"],
        env=environment, text=True,
    ).strip()).resolve(strict=True)
    copy_sdk_slice(sdk, destination)
