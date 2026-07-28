---
type: source
tags:
  - provenance
  - orlix-tcti
  - arm-asl
updated: 2026-07-28
status: current
summary: "Official Arm source, release, digest, and license findings for the AARCHMRS 2026-06 shared-ASL dependency."
sources:
  - "https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-A64-ISA/ISA_A64/ISA_A64_xml_A_profile-2026-06.tar.gz"
  - "https://developer.arm.com/documentation/ddi0602/2026-06/"
  - "https://developer.arm.com/documentation/ddi0602/2026-06/Shared-Pseudocode"
  - "https://developer.arm.com/documentation/ddi0602/2026-06/Proprietary-Notice"
  - "https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-OS-Machine-Readable-Data/AARCHMRS_BSD/AARCHMRS_OPENSOURCE_A_profile_FAT-2026-06.tar.gz"
---

# Official Arm shared-ASL source research

## Finding

Arm publishes a separately pin-able `2026-06` A64 ISA XML archive at
[`ISA_A64_xml_A_profile-2026-06.tar.gz`](https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-A64-ISA/ISA_A64/ISA_A64_xml_A_profile-2026-06.tar.gz).
The archive contains the shared-ASL corpus as
`ISA_A64_xml_A_profile-2026-06/shared_pseudocode.xml`, its
`sharedps.dtd`, the per-instruction XML documents, and the package's
`notice.xml`. Arm's
[versioned DDI0602 document](https://developer.arm.com/documentation/ddi0602/2026-06/)
independently identifies the document as the `2026-06` "Arm A-profile A64
Instruction Set Architecture" and exposes its
[Shared Pseudocode](https://developer.arm.com/documentation/ddi0602/2026-06/Shared-Pseudocode)
index.

The exact archive downloaded from Arm on 2026-07-28 has these identities:

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| `ISA_A64_xml_A_profile-2026-06.tar.gz` | 34,182,920 bytes | `63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354` |
| `ISA_A64_xml_A_profile-2026-06/shared_pseudocode.xml` | 3,411,914 bytes | `21edeadbc26408a35bcf7315bfbcbb8a69e9e6d86d952c80b153d273f0f2349f` |
| `ISA_A64_xml_A_profile-2026-06/sharedps.dtd` | 2,664 bytes | `84fd11a935d6e51a0b866f4982332d34f3b3fe9a0c03b1b3e9cac0a56d472395` |
| `ISA_A64_xml_A_profile-2026-06/notice.xml` | 5,212 bytes | `9c2cc480e9706819e0d295329cf7f94f9256c610dfa9bc8f055a05389f74704a` |

These are locally computed digests of bytes fetched from the cited Arm URL;
they are not represented as Arm-published checksums. A second download from
that URL was byte-identical to the existing local candidate archive. The
shared XML declares 1,831 `<ps>` entries.

## Compatibility with AARCHMRS 2026-06

Arm's official
[`AARCHMRS_OPENSOURCE_A_profile_FAT-2026-06.tar.gz`](https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-OS-Machine-Readable-Data/AARCHMRS_BSD/AARCHMRS_OPENSOURCE_A_profile_FAT-2026-06.tar.gz)
has SHA-256
`fe458c521745cc2d07244c416106add69ae8faa12fa3da8aad34fa76ab29399a`.
Its `Instructions.json` metadata identifies architecture `vFATAp1-A`, build
`818`, reference `2026-06_rel`, schema `2.9.5`, and timestamp
`2026-06-24 17:12:14`. The package README says its architectural JSON content
has the same quality as the equivalent XML releases.

[INFERENCE] The official `2026-06` AARCHMRS release and official `2026-06` A64
ISA XML release are the matching Arm release pair. The matching release
identifiers, Arm's equivalence statement, and the DDI0602 versioned Shared
Pseudocode index provide source-backed compatibility evidence. The XML archive
must remain the provenance unit because Arm does not publish
`shared_pseudocode.xml` as an independently versioned download.

## License result

The AARCHMRS JSON archive states BSD-3-Clause terms for its machine-readable
JSON content. Those terms do not extend to the separate A64 ISA XML archive.
The XML archive's `notice.xml` matches Arm's versioned
[Proprietary Notice](https://developer.arm.com/documentation/ddi0602/2026-06/Proprietary-Notice):
the document is non-confidential, but Arm reserves reproduction and states that
the document itself grants no intellectual-property license unless one is
specifically stated. "Non-confidential" therefore does not mean open source or
redistributable.

[INFERENCE] The XML archive satisfies the official-source, version-compatibility,
and digest parts of issue #112, but the package alone does not satisfy its
licensed-use criterion. Orlix needs an applicable pre-existing Arm agreement or
express written permission covering use and any required retention or
distribution of this corpus. Until that authorization is verified, the archive
can be pinned as external technical provenance but must not be redistributed or
treated as an implementation license.

I cannot verify any redistributable license for this official shared-ASL corpus
from the package or Arm's versioned public documentation.

## Coverage limitation

The matching DDI0602 XML archive has semantic encoding entries for 4,332 of the
4,350 pinned AARCHMRS leaves. It has no instruction XML entry for ordinals
2235, 2302, 2308 through 2311, and 2675 through 2686. These are `TENTER`,
`TEXIT`, `TCHANGEB`, `TCHANGEF`, and `SETGO*` encodings. Their AARCHMRS
operation objects explicitly contain `operation: "// Not specified"` and a
null decode. They are feature-applicable leaves, not typed inapplicability.

The exact AARCHMRS operation source span and digest can prove that Arm omitted
the semantics from this release. It cannot define the missing expected behavior
or grant implementation, proof, or runtime-capability credit. Those 18 rows
remain semantic-completion blockers until an official compatible semantic
source exists.

## Recommended source identity

Use the Arm archive URL, archive SHA-256, release directory, and
`shared_pseudocode.xml` member SHA-256 together as the immutable shared-ASL
identity. Retain `sharedps.dtd` and `notice.xml` digests alongside it. Use the
versioned DDI0602 pages as an official online cross-check, not as the byte-level
pin. No QEMU, LLVM, Sail, generated model output, or third-party mirror is
needed to establish this source identity.
