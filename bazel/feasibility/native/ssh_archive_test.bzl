"""Analysis tests for separate OpenSSL and libssh2 Apple archives."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")

_SLICES = (
    ("ios-device", "arm64"),
    ("ios-simulator", "arm64"),
    ("macos", "arm64"),
)

def _slice_marker(destination, architecture):
    return "ssh_archives/%s/%s" % (destination, architecture)

def _slice_of(path):
    for destination, architecture in _SLICES:
        if _slice_marker(destination, architecture) in path:
            return destination, architecture
    return None

def _paths(files):
    return [f.path for f in files.to_list()]

def _owned_slice(env, action):
    found = None
    for path in _paths(action.outputs):
        owned = _slice_of(path)
        asserts.true(env, owned != None, path)
        if found == None:
            found = owned
        else:
            asserts.equals(env, found, owned)
    asserts.true(env, found != None)
    return found

def _reject_foreign_slices(env, paths, destination, architecture):
    owned = _slice_marker(destination, architecture)
    for path in paths:
        for other_destination, other_architecture in _SLICES:
            marker = _slice_marker(other_destination, other_architecture)
            if marker == owned:
                continue
            asserts.false(
                env,
                marker in path,
                "archive %s depends on %s via %s" % (owned, marker, path),
            )

def _ssh_archive_inputs_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    openssl_actions = []
    libssh2_actions = []
    for action in target.actions:
        asserts.false(env, action.mnemonic == "OrlixSshAppleArchives")
        if action.mnemonic == "OrlixOpenSSLAppleArchive":
            openssl_actions.append(action)
        elif action.mnemonic == "OrlixLibssh2AppleArchive":
            libssh2_actions.append(action)
    asserts.equals(env, len(_SLICES), len(openssl_actions))
    asserts.equals(env, len(_SLICES), len(libssh2_actions))
    openssl_slices = {}
    for action in openssl_actions:
        destination, architecture = _owned_slice(env, action)
        openssl_slices["%s/%s" % (destination, architecture)] = True
        paths = _paths(action.inputs)
        _reject_foreign_slices(env, paths, destination, architecture)
        joined = " ".join(paths)
        asserts.true(env, "orlix_openssl_source" in joined, joined)
        asserts.false(env, "orlix_libssh2_source" in joined)
        asserts.false(env, "orlix_cmake_tool" in joined)
        asserts.false(env, "GhosttyKit.xcframework" in joined)
        for path in paths:
            asserts.false(env, path.endswith(".swift"), path)
        slice_inputs = [path for path in paths if path.endswith(".slice")]
        asserts.equals(env, 1, len(slice_inputs))
        asserts.true(env, _slice_marker(destination, architecture) + ".slice" in slice_inputs[0])
    libssh2_slices = {}
    for action in libssh2_actions:
        destination, architecture = _owned_slice(env, action)
        libssh2_slices["%s/%s" % (destination, architecture)] = True
        paths = _paths(action.inputs)
        _reject_foreign_slices(env, paths, destination, architecture)
        joined = " ".join(paths)
        marker = _slice_marker(destination, architecture)
        asserts.true(env, "orlix_libssh2_source" in joined)
        asserts.true(env, marker + "/libssl.a" in joined)
        asserts.true(env, marker + "/libcrypto.a" in joined)
        asserts.true(env, marker + "/openssl-include" in joined)
        asserts.false(env, "orlix_openssl_source" in joined, "libssh2 inputs include OpenSSL sources")
        asserts.false(env, "GhosttyKit.xcframework" in joined)
        for path in paths:
            asserts.false(env, path.endswith(".swift"), path)
            asserts.false(env, path.endswith("/Configure") and "openssl" in path, path)
        slice_inputs = [path for path in paths if path.endswith(".slice")]
        asserts.equals(env, 1, len(slice_inputs))
        asserts.true(env, marker + ".slice" in slice_inputs[0])
    for destination, architecture in _SLICES:
        key = "%s/%s" % (destination, architecture)
        asserts.true(env, key in openssl_slices, key)
        asserts.true(env, key in libssh2_slices, key)
    return analysistest.end(env)

ssh_archive_inputs_test = analysistest.make(_ssh_archive_inputs_test_impl)

def _ghostty_archive_immutable_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    files = target.files.to_list()
    asserts.true(env, len(files) > 0)
    for file in files:
        asserts.true(env, "orlix_ghostty_kit" in file.path, file.path)
        asserts.true(env, "GhosttyKit.xcframework" in file.path, file.path)
        asserts.false(env, file.path.endswith(".swift"), file.path)
    for action in target.actions:
        asserts.false(env, action.mnemonic in ["SwiftCompile", "CppCompile", "ObjcCompile"])
        for file in action.inputs.to_list() + action.outputs.to_list():
            asserts.false(env, file.path.endswith(".swift"), file.path)
            asserts.true(
                env,
                "GhosttyKit.xcframework" in file.path or "orlix_ghostty_kit" in file.path,
                file.path,
            )
    return analysistest.end(env)

ghostty_archive_immutable_test = analysistest.make(_ghostty_archive_immutable_test_impl)
