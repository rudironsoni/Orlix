"""Product Xcode pin versus allowed local cache identities."""

from __future__ import annotations

import json
import hashlib
import os
import platform
import subprocess
import shutil
from pathlib import Path

PIN_PATH = Path(__file__).with_name("toolchain-pin.json")


class PinError(RuntimeError):
    pass


def load_pin(path: Path = PIN_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def product_pin(data: dict | None = None) -> dict:
    pin = (data or load_pin())["product_pin"]
    if pin["xcode_version"] == "27.0":
        raise PinError("Xcode 27.0 is not the product pin")
    return pin


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
    pin = product_pin(payload)
    if version == "27.0" and pin["xcode_version"] != "26.6":
        raise PinError("Xcode 27.0 is not the product pin")
    try:
        expected = namespace_for(version, build, payload)
    except PinError:
        raise PinError(f"unsupported Xcode identity {version} {build}")
    if expected not in disk_cache:
        raise PinError(
            f"disk cache {disk_cache} is not namespaced as {expected}"
        )


def capture_manifest(developer_dir: str, bazel: str, output: str) -> dict:
    developer = str(Path(developer_dir).resolve(strict=True))
    env = {**os.environ, "DEVELOPER_DIR": developer}

    def observe(*command: str) -> str:
        return subprocess.run(
            command, env=env, check=True, capture_output=True, text=True, timeout=30,
        ).stdout.strip()

    xcode = observe("/usr/bin/xcodebuild", "-version").splitlines()
    version = xcode[0].removeprefix("Xcode ")
    build = xcode[1].removeprefix("Build version ")
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
    destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return payload


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
    mlibc = capture_mlibc_manifest(developer, sdk, runtime, tree_hashes)
    tools += [Path(path) for path in mlibc["files"]]
    trees += [Path(path) for path in mlibc["trees"]]
    Path(output).with_name("mlibc-identity.json").write_text(json.dumps(mlibc, sort_keys=True) + "\n")
    compiler = {**runtime, "files": {name: digest for name, digest in runtime["files"].items() if name.endswith("/clang")}}
    Path(output).with_name("guest-compiler-identity.json").write_text(json.dumps(compiler, sort_keys=True) + "\n")
    candidates = [str(Path(directory) / name) for directory in search_path.split(":") for name in names]
    return {"files": sorted(set([str(path) for path in tools] + candidates)), "trees": [str(path) for path in trees]}


def capture_mlibc_manifest(developer: Path, sdk: Path, runtime: dict, tree_hashes: dict) -> dict:
    meson = Path("/opt/homebrew/bin/meson")
    interpreter = Path(meson.read_text().splitlines()[0].removeprefix("#!"))
    modules = json.loads(subprocess.check_output([
        str(interpreter), "-I", "-B", "-c",
        "import json,mesonbuild,sysconfig; print(json.dumps([list(mesonbuild.__path__)[0],sysconfig.get_path('stdlib')]))",
    ], text=True))
    action_python, action_stdlib = json.loads(subprocess.check_output([
        "/usr/bin/python3", "-B", "-c",
        "import json,sys,sysconfig; print(json.dumps([sys.executable,sysconfig.get_path('stdlib')]))",
    ], env={**os.environ, "DEVELOPER_DIR": str(developer)}, text=True))
    modules.append(action_stdlib)
    paths = {
        Path(name) for name in runtime["files"]
    } | {
        interpreter, Path(action_python), Path("/opt/homebrew/bin/ninja"),
        developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/ld",
        Path("/opt/homebrew/opt/lld/bin/ld.lld"),
        Path("/opt/homebrew/opt/llvm/bin/llvm-strip"),
        Path("/opt/homebrew/bin/ccache"),
        developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++",
    }
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
                    raise PinError(f"unresolved mlibc tool dependency: {name} from {binary}")
            else:
                path = Path(name.replace("@loader_path", str(binary.resolve().parent)).replace("@executable_path", str(binary.resolve().parent)))
            if not path.is_absolute():
                raise PinError(f"unresolved mlibc tool dependency: {name}")
            if path not in paths:
                paths.add(path)
                pending.append(path)
    paths |= {meson, Path("/usr/bin/python3"), Path("/usr/bin/patch"), Path("/bin/bash"),
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
