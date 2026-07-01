# orlix-a64-opprofile

Planned opcode inventory tool for Orlix TCTI.

The tool must report AArch64 decode-class coverage for:

- static `/init`
- dynamic loader
- busybox/toybox
- apk
- OrlixMLibC test binaries
- coreutils sample binaries

Instruction support must expand from observed traces and focused tests, not from broad guessing.
