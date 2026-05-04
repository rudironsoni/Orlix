# Vendored Linux Headers

IXLandSystem vendors three distinct Linux-shaped header surfaces under third_party/linux:

- `uapi/<version>/<arch>/include`: exported userspace headers from Linux `headers_install`.
- `kheaders/<version>/<arch>/srctree` and `objtree`: Linux source and generated kernel-internal header roots.
- `abi/<version>/<arch>/include`: IXLandSystem-owned ABI supplement headers derived from Linux source when required outside exported UAPI.

Regenerate the vendored tree with:

```sh
make vendor-linux-headers LINUX_VERSION=<version> LINUX_ARCH=<arch>
```

Do not hand-edit vendored Linux headers. Regenerate them from pristine upstream Linux sources through the Makefile pipeline.
