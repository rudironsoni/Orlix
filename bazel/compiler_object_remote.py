"""Content-addressed remote store for ccache's Bazel HTTP layout.

ccache addresses each result by its compilation digest and stores the opaque
entry at ``/ac/<64 hex>``. This module serves that layout and can put the
bytes in the GitHub Actions cache. It does not keep a directory of compiler
objects, and it does not speak BuildBuddy's action cache.
"""

from __future__ import annotations

import hashlib
import json
import re
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Protocol


_DIGEST = re.compile(r"[0-9a-f]{64}\Z")
CACHE_VERSION = hashlib.sha256(b"orlix-kernel-compiler-object-v5").hexdigest()
_KEY_PREFIX = "orlix-kobj-v5-"
_CHUNK = 16 * 1024 * 1024


class BlobStore(Protocol):
    allow_write: bool
    read_bytes: int
    write_bytes: int

    def get(self, digest: str) -> bytes | None: ...

    def head(self, digest: str) -> bool: ...

    def put(self, digest: str, blob: bytes) -> None: ...


class MemoryBlobStore:
    """In-process stand-in used by the protocol test. Not a remote backend."""

    def __init__(self, *, allow_write: bool) -> None:
        self.allow_write = allow_write
        self.read_bytes = 0
        self.write_bytes = 0
        self._blobs: dict[str, bytes] = {}
        self._lock = threading.Lock()

    def get(self, digest: str) -> bytes | None:
        _require_digest(digest)
        with self._lock:
            blob = self._blobs.get(digest)
            if blob is not None:
                self.read_bytes += len(blob)
            return blob

    def head(self, digest: str) -> bool:
        _require_digest(digest)
        with self._lock:
            return digest in self._blobs

    def put(self, digest: str, blob: bytes) -> None:
        _require_digest(digest)
        if not self.allow_write:
            raise PermissionError("compiler object remote store is read-only")
        with self._lock:
            self._blobs[digest] = blob
            self.write_bytes += len(blob)


class ActionsBlobStore:
    """GitHub Actions cache entries keyed by the ccache digest."""

    def __init__(
        self,
        cache_url: str,
        token: str,
        *,
        allow_write: bool,
        service_v2: bool = False,
    ) -> None:
        if not cache_url or not token:
            raise RuntimeError(
                "GitHub Actions cache runtime credentials are required "
                "(ACTIONS_CACHE_URL or ACTIONS_RESULTS_URL, and ACTIONS_RUNTIME_TOKEN)"
            )
        self.cache_url = cache_url if cache_url.endswith("/") else cache_url + "/"
        self.token = token
        self.allow_write = allow_write
        self.service_v2 = service_v2
        self.read_bytes = 0
        self.write_bytes = 0
        self.version = CACHE_VERSION
        self._lock = threading.Lock()

    def get(self, digest: str) -> bytes | None:
        location = self._lookup(digest)
        if location is None:
            return None
        blob = _download(location)
        with self._lock:
            self.read_bytes += len(blob)
        return blob

    def head(self, digest: str) -> bool:
        return self._lookup(digest) is not None

    def put(self, digest: str, blob: bytes) -> None:
        _require_digest(digest)
        if not self.allow_write:
            raise PermissionError("compiler object remote store is read-only")
        key = _cache_key(digest)
        last_error: Exception | None = None
        for attempt in range(5):
            try:
                if self.service_v2:
                    self._put_v2(key, blob)
                else:
                    self._put_once(key, blob)
                with self._lock:
                    self.write_bytes += len(blob)
                return
            except _AlreadyStored:
                return
            except Exception as error:  # noqa: BLE001 - retry the cache API
                last_error = error
                time.sleep(0.4 * (attempt + 1))
        raise RuntimeError(f"uploading compiler object {digest} failed") from last_error

    def _put_v2(self, key: str, blob: bytes) -> None:
        created = self._twirp("CreateCacheEntry", {"key": key, "version": self.version})
        upload = _field(created, "signed_upload_url", "signedUploadUrl")
        if not created.get("ok") or not upload:
            if self._lookup_v2(key):
                raise _AlreadyStored
            message = _field(created, "message") or "reservation was not accepted"
            raise RuntimeError(f"reserve failed: {message}")
        status, response = _request(
            "PUT",
            upload,
            blob,
            {"Content-Type": "application/octet-stream", "x-ms-blob-type": "BlockBlob"},
        )
        if status not in (200, 201):
            raise RuntimeError(f"upload failed: {status} {_safe_text(response)}")
        finalized = self._twirp(
            "FinalizeCacheEntryUpload",
            {"key": key, "version": self.version, "size_bytes": str(len(blob))},
        )
        if not finalized.get("ok"):
            message = _field(finalized, "message") or "finalize was not accepted"
            raise RuntimeError(f"commit failed: {message}")

    def _put_once(self, key: str, blob: bytes) -> None:
        reserve = json.dumps(
            {"key": key, "version": self.version, "cacheSize": len(blob)}
        ).encode()
        status, body = self._api(
            "POST",
            "caches",
            reserve,
            {"Content-Type": "application/json"},
        )
        if status in (400, 409):
            if self._lookup_key(key) is not None:
                raise _AlreadyStored
            raise RuntimeError(f"reserve failed: {status} {_safe_text(body)}")
        if status not in (200, 201):
            raise RuntimeError(f"reserve failed: {status} {_safe_text(body)}")
        cache_id = json.loads(body)["cacheId"]
        offset = 0
        while offset < len(blob):
            chunk = blob[offset : offset + _CHUNK]
            end = offset + len(chunk) - 1
            status, response = self._api(
                "PATCH",
                f"caches/{cache_id}",
                chunk,
                {
                    "Content-Type": "application/octet-stream",
                    "Content-Range": f"bytes {offset}-{end}/*",
                },
            )
            if status not in (200, 201, 204):
                raise RuntimeError(f"upload failed: {status} {_safe_text(response)}")
            offset = end + 1
        commit = json.dumps({"size": len(blob)}).encode()
        status, response = self._api(
            "POST",
            f"caches/{cache_id}",
            commit,
            {"Content-Type": "application/json"},
        )
        if status not in (200, 201, 204):
            raise RuntimeError(f"commit failed: {status} {_safe_text(response)}")

    def _lookup(self, digest: str) -> str | None:
        return self._lookup_key(_cache_key(digest))

    def _lookup_key(self, key: str) -> str | None:
        if self.service_v2:
            return self._lookup_v2(key)
        quoted = urllib.parse.quote(key, safe="")
        status, body = self._api("GET", f"cache?keys={quoted}&version={self.version}")
        if status == 204:
            return None
        if status != 200:
            raise RuntimeError(f"cache lookup failed: {status} {_safe_text(body)}")
        location = json.loads(body).get("archiveLocation")
        if not location:
            raise RuntimeError("cache lookup returned no archive")
        return str(location)

    def _lookup_v2(self, key: str) -> str | None:
        found = self._twirp(
            "GetCacheEntryDownloadURL",
            {"key": key, "version": self.version, "restore_keys": []},
        )
        if not found.get("ok"):
            return None
        location = _field(found, "signed_download_url", "signedDownloadUrl")
        return location or None

    def _twirp(self, method: str, payload: dict[str, object]) -> dict[str, object]:
        parsed = urllib.parse.urlsplit(self.cache_url)
        url = urllib.parse.urlunsplit(
            (
                parsed.scheme,
                parsed.netloc,
                f"/twirp/github.actions.results.api.v1.CacheService/{method}",
                "",
                "",
            )
        )
        status, body = _request(
            "POST",
            url,
            json.dumps(payload).encode(),
            {
                "Accept": "application/json",
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
                "User-Agent": "orlix-compiler-object-remote",
            },
        )
        if status != 200:
            raise RuntimeError(f"{method} failed: {status} {_safe_text(body)}")
        decoded = json.loads(body)
        if not isinstance(decoded, dict):
            raise RuntimeError(f"{method} returned a non-object")
        return decoded

    def _api(
        self,
        method: str,
        resource: str,
        data: bytes | None = None,
        extra: dict[str, str] | None = None,
    ) -> tuple[int, bytes]:
        headers = {
            "Accept": "application/json;api-version=6.0-preview.1",
            "Authorization": f"Bearer {self.token}",
            "User-Agent": "orlix-compiler-object-remote",
        }
        if extra:
            headers.update(extra)
        return _request(method, self.cache_url + "_apis/artifactcache/" + resource, data, headers)


class _AlreadyStored(Exception):
    pass


class _Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802
        self._read(send_body=True)

    def do_HEAD(self) -> None:  # noqa: N802
        self._read(send_body=False)

    def do_PUT(self) -> None:  # noqa: N802
        try:
            digest = _digest_from_path(self.path)
        except ValueError:
            self._empty(400)
            return
        length = int(self.headers.get("Content-Length", "0"))
        blob = self.rfile.read(length)
        if not self.server.allow_write:  # type: ignore[attr-defined]
            # ccache updates a manifest after a direct hit. Accept that write
            # without storing it so the hit still stands.
            self._empty(200)
            return
        try:
            self.server.store.put(digest, blob)  # type: ignore[attr-defined]
        except PermissionError:
            self._empty(403)
            return
        except Exception:  # noqa: BLE001 - the client must see a failed store
            self._empty(500)
            return
        self._empty(200)

    def do_DELETE(self) -> None:  # noqa: N802
        # Entries are content-addressed. Leaving the blob in place is the
        # same result ccache would read back.
        self._empty(200)

    def log_message(self, format: str, *args: object) -> None:
        return

    def _read(self, *, send_body: bool) -> None:
        try:
            digest = _digest_from_path(self.path)
        except ValueError:
            self._empty(400)
            return
        try:
            if send_body:
                blob = self.server.store.get(digest)  # type: ignore[attr-defined]
            else:
                blob = b"" if self.server.store.head(digest) else None  # type: ignore[attr-defined]
        except Exception:  # noqa: BLE001
            self._empty(500)
            return
        if blob is None:
            self._empty(404)
            return
        if not send_body:
            present = self.server.store.head(digest)  # type: ignore[attr-defined]
            self._empty(200 if present else 404)
            return
        body = blob
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _empty(self, status: int) -> None:
        self.send_response(status)
        self.send_header("Content-Length", "0")
        self.end_headers()


def serve(store: BlobStore) -> tuple[ThreadingHTTPServer, int]:
    """Listen on loopback and return the server plus its port."""
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
    httpd.store = store  # type: ignore[attr-defined]
    httpd.allow_write = store.allow_write  # type: ignore[attr-defined]
    thread = threading.Thread(target=httpd.serve_forever, name="compiler-object-remote", daemon=True)
    thread.start()
    return httpd, int(httpd.server_address[1])


def actions_runtime(environ: dict[str, str]) -> tuple[str, str, bool]:
    """Return the cache service URL, token, and whether service v2 is active.

    The runner injects these into JavaScript actions. A `run` step only sees
    them after that action copies them into the job environment.
    """
    service_v2 = bool(environ.get("ACTIONS_CACHE_SERVICE_V2"))
    if service_v2:
        url = environ.get("ACTIONS_RESULTS_URL", "")
    else:
        url = environ.get("ACTIONS_CACHE_URL") or environ.get("ACTIONS_RESULTS_URL") or ""
    token = environ.get("ACTIONS_RUNTIME_TOKEN", "")
    if not url or not token:
        raise RuntimeError(
            "GitHub Actions cache runtime credentials are required "
            "(ACTIONS_CACHE_URL or ACTIONS_RESULTS_URL, and ACTIONS_RUNTIME_TOKEN)"
        )
    return url, token, service_v2


def remote_storage_url(port: int, *, read_only: bool) -> str:
    url = (
        f"http://127.0.0.1:{port}|layout=bazel"
        "|connect-timeout=10000|operation-timeout=60000"
    )
    if read_only:
        url += "|read-only=true"
    return url


def _require_digest(digest: str) -> None:
    if _DIGEST.fullmatch(digest) is None:
        raise ValueError(f"compiler object digest must be 64 hex digits, got {digest!r}")


def _digest_from_path(path: str) -> str:
    name = urllib.parse.urlsplit(path).path
    prefix = "/ac/"
    if not name.startswith(prefix):
        raise ValueError(path)
    digest = name[len(prefix) :]
    _require_digest(digest)
    return digest


def _cache_key(digest: str) -> str:
    _require_digest(digest)
    return _KEY_PREFIX + digest


def _field(payload: dict[str, object], *names: str) -> str:
    for name in names:
        value = payload.get(name)
        if value:
            return str(value)
    return ""


def _safe_text(body: bytes) -> str:
    text = body.decode("utf-8", "replace")
    return text[:300].replace("\n", " ")


def _request(
    method: str,
    url: str,
    data: bytes | None,
    headers: dict[str, str],
) -> tuple[int, bytes]:
    request = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def _download(url: str) -> bytes:
    request = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()
