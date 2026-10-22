"""Protocol tests for the compiler-object remote store.

The memory store and the fake Actions endpoint are not the kernel proof.
The gcc case checks that ccache's Bazel HTTP layout can restore one object
on a second directory without executing gcc. The kernel proof is the
ubuntu workflow.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from bazel.ci.kernel_object_remote_reuse import (
    ccache_environment,
    compiler_executions,
    differing_headers,
    parse_print_stats,
    remote_counters,
)
from bazel.compiler_object_remote import (
    ActionsBlobStore,
    MemoryBlobStore,
    actions_runtime,
    remote_storage_url,
    serve,
)


ROOT = Path(__file__).resolve().parents[2]
_DIGEST = "ab" * 32


class _ActionsHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        server = self.server
        if self.path.endswith("/caches") and "/caches/" not in self.path:
            payload = json.loads(body)
            if payload["key"] in server.entries:  # type: ignore[attr-defined]
                self._send(409, b'{"message":"already exists"}')
                return
            server.next_id += 1  # type: ignore[attr-defined]
            cache_id = server.next_id  # type: ignore[attr-defined]
            server.pending[cache_id] = {"key": payload["key"], "chunks": [], "size": payload["cacheSize"]}  # type: ignore[attr-defined]
            self._send(201, json.dumps({"cacheId": cache_id}).encode())
            return
        cache_id = int(self.path.rstrip("/").rsplit("/", 1)[-1])
        pending = server.pending.pop(cache_id)  # type: ignore[attr-defined]
        blob = b"".join(pending["chunks"])
        if len(blob) != pending["size"] or json.loads(body)["size"] != len(blob):
            self._send(400, b'{"message":"size"}')
            return
        server.entries[pending["key"]] = blob  # type: ignore[attr-defined]
        self._send(200, b"{}")

    def do_PATCH(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        chunk = self.rfile.read(length)
        cache_id = int(self.path.rstrip("/").rsplit("/", 1)[-1])
        self.server.pending[cache_id]["chunks"].append(chunk)  # type: ignore[attr-defined]
        self._send(204, b"")

    def do_GET(self) -> None:  # noqa: N802
        server = self.server
        path = self.path
        if path.startswith("/blob/"):
            key = path[len("/blob/") :]
            blob = server.entries.get(key)  # type: ignore[attr-defined]
            if blob is None:
                self._send(404, b"")
                return
            self._send(200, blob, "application/octet-stream")
            return
        if "keys=" not in path:
            self._send(404, b"")
            return
        key = path.split("keys=", 1)[1].split("&", 1)[0]
        key = urllib_unquote(key)
        blob = server.entries.get(key)  # type: ignore[attr-defined]
        if blob is None:
            self._send(204, b"")
            return
        location = f"http://127.0.0.1:{server.server_address[1]}/blob/{key}"  # type: ignore[attr-defined]
        self._send(200, json.dumps({"archiveLocation": location}).encode())

    def log_message(self, format: str, *args: object) -> None:
        return

    def _send(self, status: int, body: bytes, content_type: str = "application/json") -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)


class _ActionsV2Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length))
        server = self.server
        if self.path.endswith("/CreateCacheEntry"):
            key = payload["key"]
            if key in server.entries:  # type: ignore[attr-defined]
                self._json({"ok": False, "message": "already exists"})
                return
            self._json(
                {
                    "ok": True,
                    "signed_upload_url": f"http://127.0.0.1:{server.server_address[1]}/upload/{key}",  # type: ignore[attr-defined]
                }
            )
            return
        if self.path.endswith("/FinalizeCacheEntryUpload"):
            key = payload["key"]
            blob = server.pending.get(key)  # type: ignore[attr-defined]
            if blob is None or str(len(blob)) != payload["size_bytes"]:
                self._json({"ok": False, "message": "size"})
                return
            server.entries[key] = blob  # type: ignore[attr-defined]
            self._json({"ok": True, "entry_id": "1"})
            return
        if self.path.endswith("/GetCacheEntryDownloadURL"):
            key = payload["key"]
            blob = server.entries.get(key)  # type: ignore[attr-defined]
            if blob is None:
                self._json({"ok": False, "signed_download_url": ""})
                return
            self._json(
                {
                    "ok": True,
                    "signed_download_url": f"http://127.0.0.1:{server.server_address[1]}/blob/{key}",  # type: ignore[attr-defined]
                    "matched_key": key,
                }
            )
            return
        self._json({"ok": False, "message": "unknown"}, status=404)

    def do_PUT(self) -> None:  # noqa: N802
        if self.headers.get("x-ms-blob-type") != "BlockBlob":
            self._json({"message": "blob type"}, status=400)
            return
        length = int(self.headers.get("Content-Length", "0"))
        key = self.path.rsplit("/", 1)[-1]
        self.server.pending[key] = self.rfile.read(length)  # type: ignore[attr-defined]
        self._send(201, b"")

    def do_GET(self) -> None:  # noqa: N802
        key = self.path.rsplit("/", 1)[-1]
        blob = self.server.entries.get(key)  # type: ignore[attr-defined]
        if blob is None:
            self._send(404, b"")
            return
        self._send(200, blob)

    def log_message(self, format: str, *args: object) -> None:
        return

    def _json(self, payload: dict[str, object], status: int = 200) -> None:
        self._send(status, json.dumps(payload).encode())

    def _send(self, status: int, body: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)


def _actions_v2_server() -> tuple[ThreadingHTTPServer, int]:
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), _ActionsV2Handler)
    httpd.entries = {}  # type: ignore[attr-defined]
    httpd.pending = {}  # type: ignore[attr-defined]
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd, int(httpd.server_address[1])


def urllib_unquote(text: str) -> str:
    from urllib.parse import unquote

    return unquote(text)


def _actions_server() -> tuple[ThreadingHTTPServer, int]:
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), _ActionsHandler)
    httpd.entries = {}  # type: ignore[attr-defined]
    httpd.pending = {}  # type: ignore[attr-defined]
    httpd.next_id = 0  # type: ignore[attr-defined]
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd, int(httpd.server_address[1])


class CompilerObjectRemoteTests(unittest.TestCase):
    def test_memory_store_round_trip_and_read_only_rejection(self) -> None:
        store = MemoryBlobStore(allow_write=True)
        httpd, port = serve(store)
        digest = hashlib.sha256(b"object").hexdigest()
        url = remote_storage_url(port, read_only=False)
        self.assertIn("|layout=bazel", url)
        self.assertNotIn("read-only=true", url)
        self.assertIn("read-only=true", remote_storage_url(port, read_only=True))
        try:
            self._request("PUT", f"http://127.0.0.1:{port}/ac/{digest}", b"object-bytes")
            self.assertEqual(self._request("GET", f"http://127.0.0.1:{port}/ac/{digest}"), b"object-bytes")
            status, _ = self._raw("HEAD", f"http://127.0.0.1:{port}/ac/{digest}")
            self.assertEqual(status, 200)
            self.assertGreater(store.read_bytes, 0)
            self.assertGreater(store.write_bytes, 0)
            status, _ = self._raw("GET", f"http://127.0.0.1:{port}/missing")
            self.assertEqual(status, 400)
            missing = "cd" * 32
            status, _ = self._raw("GET", f"http://127.0.0.1:{port}/ac/{missing}")
            self.assertEqual(status, 404)
            httpd.allow_write = False
            store.allow_write = False
            status, _ = self._raw("PUT", f"http://127.0.0.1:{port}/ac/{digest}", b"nope")
            self.assertEqual(status, 200)
            self.assertEqual(self._request("GET", f"http://127.0.0.1:{port}/ac/{digest}"), b"object-bytes")
            status, _ = self._raw("DELETE", f"http://127.0.0.1:{port}/ac/{digest}")
            self.assertEqual(status, 200)
        finally:
            httpd.shutdown()

    def test_runtime_selects_results_url_for_cache_service_v2(self) -> None:
        with self.assertRaises(RuntimeError):
            actions_runtime({})
        url, token, service_v2 = actions_runtime(
            {
                "ACTIONS_CACHE_SERVICE_V2": "true",
                "ACTIONS_CACHE_URL": "https://cache.example/v1/",
                "ACTIONS_RESULTS_URL": "https://results.example/v2/",
                "ACTIONS_RUNTIME_TOKEN": "token",
            }
        )
        self.assertEqual(url, "https://results.example/v2/")
        self.assertEqual(token, "token")
        self.assertTrue(service_v2)

    def test_actions_store_v2_uploads_through_the_signed_blob_url(self) -> None:
        httpd, port = _actions_v2_server()
        try:
            store = ActionsBlobStore(
                f"http://127.0.0.1:{port}/ignored/",
                "runtime-token",
                allow_write=True,
                service_v2=True,
            )
            store.put(_DIGEST, b"kernel-object")
            self.assertEqual(store.get(_DIGEST), b"kernel-object")
            self.assertEqual(store.write_bytes, len(b"kernel-object"))
            store.put(_DIGEST, b"kernel-object")
            self.assertEqual(store.write_bytes, len(b"kernel-object"))
        finally:
            httpd.shutdown()
            httpd.server_close()

    def test_actions_store_rejects_missing_runtime_credentials(self) -> None:
        with self.assertRaises(RuntimeError):
            ActionsBlobStore("", "", allow_write=True)
        store = ActionsBlobStore("http://127.0.0.1:9", "token", allow_write=False)
        with self.assertRaises(PermissionError):
            store.put(_DIGEST, b"blob")

    def test_actions_store_uploads_and_reads_without_a_bearer_on_the_blob(self) -> None:
        httpd, port = _actions_server()
        try:
            store = ActionsBlobStore(f"http://127.0.0.1:{port}/", "runtime-token", allow_write=True)
            store.put(_DIGEST, b"kernel-object")
            self.assertEqual(store.get(_DIGEST), b"kernel-object")
            self.assertEqual(store.read_bytes, len(b"kernel-object"))
            self.assertEqual(store.write_bytes, len(b"kernel-object"))
            store.put(_DIGEST, b"kernel-object")
            self.assertEqual(store.write_bytes, len(b"kernel-object"))
            self.assertIsNone(store.get("12" * 32))
        finally:
            httpd.shutdown()

    def test_print_stats_remote_counters_and_strace_failure(self) -> None:
        text = "\n".join(
            [
                "direct_cache_hit\t2",
                "preprocessed_cache_hit\t0",
                "remote_storage_error\t0",
                "remote_storage_hit\t2",
                "remote_storage_miss\t1",
                "remote_storage_read_hit\t2",
                "remote_storage_timeout\t0",
                "remote_storage_write\t1",
            ]
        )
        counters = parse_print_stats(text)
        remote = remote_counters(counters)
        self.assertEqual(remote["hits"], 2)
        self.assertEqual(remote["misses"], 1)
        self.assertEqual(remote["writes"], 1)
        executions = compiler_executions(
            '123 execve("/usr/bin/gcc", ["/usr/bin/gcc", "-c", "kernel/locking/mutex.c"], 0x7ff) = 0'
        )
        self.assertEqual(executions, {"kernel/locking/mutex.c": 1})
        unfinished = compiler_executions(
            '123 execve("/usr/bin/gcc", ["/usr/bin/gcc", "-c", "kernel/locking/mutex.c"], 0x7ff <unfinished ...>'
        )
        self.assertEqual(unfinished, {"kernel/locking/mutex.c": 1})
        with self.assertRaises(SystemExit):
            compiler_executions(
                '123 execve("/usr/bin/gcc", ["/usr/bin/gcc", "-c", "kernel/locking/mutex.c"...'
            )
        from bazel.ci.kernel_object_remote_reuse import ccache_source_results

        logged = ccache_source_results(
            "\n".join(
                [
                    "Command line: gcc -c kernel/locking/mutex.c",
                    "Result: cache hit (direct)",
                    "Command line: gcc -c kernel/bounds.c",
                    "Result: cache hit (preprocessed)",
                ]
            )
        )
        self.assertEqual(logged["kernel/locking/mutex.c"]["result"], "cache hit (direct)")
        self.assertNotIn("kernel/bounds.c", logged)

    def test_two_directories_reuse_one_gcc_object_through_bazel_http(self) -> None:
        if any(shutil.which(name) is None for name in ("gcc", "ccache", "strace", "nm")):
            self.skipTest("gcc, ccache, strace, and nm are required")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            store = MemoryBlobStore(allow_write=True)
            httpd, port = serve(store)
            try:
                first = root / "producer"
                self._compile(first, port, read_only=False)
                self.assertGreater(store.write_bytes, 0)
                produced = (first / "src" / "probe.o").read_bytes()
                self.assertIn(b"orlix_remote_object_probe", self._symbols(first / "src" / "probe.o"))
                httpd.allow_write = False
                store.allow_write = False
                read_before = store.read_bytes
                write_before = store.write_bytes
                second = root / "consumer"
                log = self._compile(second, port, read_only=True)
                self.assertGreater(store.read_bytes, read_before, log)
                self.assertEqual(store.write_bytes, write_before, log)
                self.assertTrue((second / "src" / "probe.d").is_file(), log)
                self.assertIn(b"orlix_remote_object_probe", self._symbols(second / "src" / "probe.o"))
            finally:
                httpd.shutdown()

    def test_workflow_keeps_write_authorization_on_the_producer(self) -> None:
        workflow = (ROOT / ".github/workflows/kernel-object-remote-reuse.yml").read_text(encoding="utf-8")
        bazel = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("pull_request:", workflow)
        self.assertIn("ORLIX_COMPILER_OBJECT_REMOTE_WRITE: \"1\"", workflow)
        self.assertEqual(workflow.count("ORLIX_COMPILER_OBJECT_REMOTE_WRITE:"), 1)
        self.assertEqual(workflow.count("uses: ./bazel/ci/compiler_object_cache_env"), 3)
        self.assertIn("actions: write", workflow)
        self.assertNotIn("ORLIX_CI_BUILDBUDDY", workflow)
        self.assertNotIn("kernel_object_remote", bazel)
        self.assertNotIn("ORLIX_COMPILER_OBJECT_REMOTE_WRITE", bazel)
        self.assertIn("__kernel-object-remote-reuse:", makefile)
        self.assertIn("ORLIX_BAZEL_AUTHORITY ?= 0", (ROOT / "Makefile").read_text(encoding="utf-8"))
        equivalence = makefile.split("__bazel-cache-equivalence:", 1)[1].split("__bazel-scenario-harness:", 1)[0]
        self.assertNotIn("__kernel-object-remote-reuse", equivalence)
        env = ccache_environment(work=ROOT, ccache_dir=ROOT / "ccache", port=9, read_only=False)
        self.assertEqual(env["CCACHE_NOINODECACHE"], "true")
        self.assertNotIn("file_stat_matches", env["CCACHE_SLOPPINESS"])
        diff = differing_headers(
            {"keep.h": {"sha256": "aa", "bytes": 1}, "move.h": {"sha256": "bb", "bytes": 2}},
            {"keep.h": {"sha256": "aa", "bytes": 1}, "move.h": {"sha256": "cc", "bytes": 3}},
        )
        self.assertEqual(diff, ["move.h consumer={'sha256': 'bb', 'bytes': 2} producer={'sha256': 'cc', 'bytes': 3}"])

    def _compile(self, work: Path, port: int, *, read_only: bool) -> str:
        source = work / "src"
        source.mkdir(parents=True)
        (source / "probe.c").write_text(
            "int orlix_remote_object_probe(void);\nint orlix_remote_object_probe(void) { return 7; }\n",
            encoding="utf-8",
        )
        ccache_dir = work / "ccache"
        ccache_dir.mkdir()
        log_path = work / "ccache.log"
        env = os.environ.copy()
        env.update(
            {
                "CCACHE_REMOTE_STORAGE": remote_storage_url(port, read_only=read_only),
                "CCACHE_REMOTE_ONLY": "true",
                "CCACHE_COMPILERCHECK": "content",
                "CCACHE_BASEDIR": str(work),
                "CCACHE_DIR": str(ccache_dir),
                "CCACHE_CONFIGPATH": "/dev/null",
                "CCACHE_NOHASHDIR": "true",
                "CCACHE_NOINODECACHE": "true",
                "CCACHE_LOGFILE": str(log_path),
                "CCACHE_SLOPPINESS": "include_file_mtime,include_file_ctime,locale",
                "LC_ALL": "C",
            }
        )
        probe = source / "probe.c"
        output = source / "probe.o"
        depfile = source / "probe.d"
        command = [
            "ccache",
            "gcc",
            "-c",
            "-MMD",
            "-MF",
            str(depfile),
            "-o",
            str(output),
            str(probe),
        ]
        strace_path = work / "compile.strace"
        completed = subprocess.run(
            ["strace", "-f", "-e", "trace=execve", "-s", "262144", "-o", str(strace_path), *command],
            cwd=source,
            env=env,
            check=False,
            text=True,
            capture_output=True,
        )
        stats = subprocess.check_output(["ccache", "--print-stats"], env=env, text=True)
        log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
        detail = f"{completed.stderr}\n{stats}\n{log}"
        self.assertEqual(completed.returncode, 0, detail)
        counters = parse_print_stats(stats)
        remote = remote_counters(counters)
        self.assertEqual(remote["errors"], 0, detail)
        self.assertEqual(remote["timeouts"], 0, detail)
        executions = compiler_executions(strace_path.read_text(encoding="utf-8", errors="replace").replace("probe.c", "kernel/locking/probe.c"))
        if read_only:
            self.assertGreaterEqual(counter_direct(counters), 1, detail)
            self.assertEqual(counters["preprocessed_cache_hit"], 0, detail)
            self.assertGreaterEqual(remote["hits"], 1, detail)
            self.assertEqual(executions, {}, detail)
        else:
            self.assertGreaterEqual(remote["misses"], 1, detail)
            self.assertGreaterEqual(remote["writes"], 1, detail)
            self.assertGreaterEqual(executions.get("kernel/locking/probe.c", 0), 1, detail)
        return detail

    def _symbols(self, path: Path) -> bytes:
        return subprocess.check_output(["nm", path])

    def _request(self, method: str, url: str, body: bytes | None = None) -> bytes:
        status, payload = self._raw(method, url, body)
        self.assertEqual(status, 200, payload)
        return payload

    def _raw(self, method: str, url: str, body: bytes | None = None) -> tuple[int, bytes]:
        request = urllib.request.Request(url, data=body, method=method)
        if body is not None:
            request.add_header("Content-Length", str(len(body)))
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                return response.status, response.read()
        except urllib.error.HTTPError as error:
            return error.code, error.read()


def counter_direct(values: dict[str, int]) -> int:
    return values["direct_cache_hit"]


if __name__ == "__main__":
    unittest.main()
