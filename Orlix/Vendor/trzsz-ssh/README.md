# TrzszSSH native transport

Orlix uses the native Go Mobile XCFramework from
[`kitknox/trzsz-ssh-rootshell`](https://github.com/kitknox/trzsz-ssh-rootshell).
The pinned source revision is `bd9e777620a2ec40e7be59ec31e8a008ff5f7fa5`
at release `v0.2.2`.

Run `make tssh-vendor-prepare` from the repository root. The Make target
downloads the `TrzszSSH.xcframework` and `VPNTunnel.xcframework` release
artifacts. It verifies SHA-256
`8290485b258da18bf5895e2ecc1c6c30d8050e6385a187a058af21402c21172c` and
`b998d313f5341db98c2b67217ce51ab46049fcb4c99c53cd6ac8f00fdd18fb0f`.
It also checks the iOS 13.0 device minimum and the iOS 13.0/14.0 simulator
minima. The generated XCFrameworks and version marker are ignored build
inputs.
