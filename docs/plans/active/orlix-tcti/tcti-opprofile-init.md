# Orlix AArch64 Opcode Profile

binary: `/Volumes/1TB/Xcode/OrlixSystem/Build/OrlixOS/rootfs/development/base-tree/sbin/init`
instructions: 54161
supported_by_current_tcti_decode: 52837
unsupported_by_current_tcti_decode: 1324

| class | count | supported |
| --- | ---: | --- |
| `load-store-unsigned-immediate` | 10102 | yes |
| `add-sub-immediate` | 9425 | yes |
| `unconditional-branch-immediate` | 5805 | yes |
| `logical-shifted-register` | 4386 | yes |
| `conditional-branch-immediate` | 3552 | yes |
| `load-store-pair` | 3276 | yes |
| `pc-relative-address` | 2775 | yes |
| `move-wide-immediate` | 2709 | yes |
| `compare-branch-immediate` | 2481 | yes |
| `add-sub-shifted-register` | 2091 | yes |
| `load-store-signed-immediate` | 1325 | yes |
| `load-store-register-offset` | 1204 | yes |
| `test-branch-immediate` | 773 | yes |
| `unsupported-brk` | 771 | no |
| `conditional-select` | 674 | yes |
| `unconditional-branch-register` | 669 | yes |
| `logical-immediate` | 462 | yes |
| `unsupported-unknown` | 405 | no |
| `bitfield` | 359 | yes |
| `hint` | 268 | yes |
| `simd` | 139 | no |
| `multiply-add-sub` | 136 | yes |
| `system-register` | 111 | yes |
| `data-processing-2source` | 105 | yes |
| `load-store-exclusive` | 50 | yes |
| `conditional-compare` | 35 | yes |
| `extract` | 30 | yes |
| `add-sub-extended-register` | 13 | yes |
| `svc` | 9 | yes |
| `unsupported-load-store-pair` | 9 | no |
| `exclusive-monitor-clear` | 7 | yes |
| `data-processing-1source` | 5 | yes |

## Unsupported Examples

### unsupported-brk (771)
- `0002361c: d4200020  brk	#0x1`
- `00025614: d4200020  brk	#0x1`
- `00025a8c: d4200020  brk	#0x1`
- `00025ad8: d4200020  brk	#0x1`
- `00026038: d4200020  brk	#0x1`
- `00026208: d4200020  brk	#0x1`
- `00026844: d4200020  brk	#0x1`
- `00026940: d4200020  brk	#0x1`
- `00026ac8: d4200020  brk	#0x1`
- `00026d58: d4200020  brk	#0x1`
- `00026e8c: d4200020  brk	#0x1`
- `000274c4: d4200020  brk	#0x1`

### unsupported-unknown (405)
- `0002254c: 9e66000a  fmov	x10, d0`
- `0004a600: 1e602008  fcmp	d0, #0.0`
- `0004a610: 1e7e1001  fmov	d1, #-1.00000000`
- `0004a614: 1e601002  fmov	d2, #2.00000000`
- `0004a620: 1f420400  fmadd	d0, d0, d2, d1`
- `0004a624: 1e602008  fcmp	d0, #0.0`
- `0004a62c: 1e6e1001  fmov	d1, #1.00000000`
- `0004a630: 1e612000  fcmp	d0, d1`
- `0004a7b8: 1e780018  fcvtzs	w24, d0`
- `0004a7bc: 1e649000  fmov	d0, #10.00000000`
- `0004a7c0: 1e649009  fmov	d9, #10.00000000`
- `0004a7c4: 1e620301  scvtf	d1, w24`

### simd (139)
- `0001b648: 4e211c01  and	v1.16b, v0.16b, v1.16b`
- `0001b64c: 4f011600  orr	v0.4s, #0x30`
- `0001b650: 6e144401  mov	v1.s[2], v0.s[2]`
- `0001e910: 4e801820  uzp1	v0.4s, v1.4s, v0.4s`
- `0001e914: 4e821861  uzp1	v1.4s, v3.4s, v2.4s`
- `0001f0ac: 0e200800  rev64	v0.8b, v0.8b`
- `0001f0c0: 0e200800  rev64	v0.8b, v0.8b`
- `0001f0d8: 0e200800  rev64	v0.8b, v0.8b`
- `000224fc: 6f00e400  movi	v0.2d, #0000000000000000`
- `00022500: 6f00e401  movi	v1.2d, #0000000000000000`
- `00022508: 4e081d40  mov	v0.d[0], x10`
- `00022510: 4e080d42  dup	v2.2d, x10`

### unsupported-load-store-pair (9)
- `0002366c: 6d0123e9  stp	d9, d8, [sp, #0x10]`
- `000255dc: 6d4123e9  ldp	d9, d8, [sp, #0x10]`
- `0004a550: 6d0d23e9  stp	d9, d8, [sp, #0xd0]`
- `0004b224: 6d4d23e9  ldp	d9, d8, [sp, #0xd0]`
- `0004b34c: 6d0a23e9  stp	d9, d8, [sp, #0xa0]`
- `0004c184: 6d4a23e9  ldp	d9, d8, [sp, #0xa0]`
- `0004d260: 6dba23e9  stp	d9, d8, [sp, #-0x60]!`
- `0004d2d4: 6cc623e9  ldp	d9, d8, [sp], #0x60`
- `0004da04: 6cc623e9  ldp	d9, d8, [sp], #0x60`
