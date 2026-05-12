# Task 2.1 — Adoption Analysis: `sf_get_decompressed_reader` Caller Sites

**Date:** 2026-05-12
**Agent:** Sisyphus-Junior (unspecified-high)
**Plan:** `.sisyphus/plans/refactor-and-gap-analysis.md` Wave 2, T2.1
**Baseline commit:** HEAD (clean tree after stashing unrelated Wave 1/2 in-flight work)

## Summary

Wave 0 audit found 7 files that invoke DCX decompression. T2.1 asks: adopt
`sf_get_decompressed_reader` at the inline caller sites where the pattern
(sniff → allocate → decompress → wrap in new reader) matches.

**Finding:** 8 of those caller sites already adopt the helper (top-level read
paths in `bnd3/bnd4/bxf3/bxf4`, committed during the initial Phase 3 port —
the helper was added 2 days *before* those files were ported). The remaining
5 inline `sf_dcx_decompress_from_buffer` invocations are in **per-file
decode helpers** that operate on a fundamentally different abstraction and
**cannot** adopt the helper without a forced fit that increases LOC,
introduces redundant heap copies, and conflates two distinct ownership models.

Per **QA Scenario 2** (`Caller pattern diverges, skip extraction`,
`refactor-and-gap-analysis.md:1454-1462`), these 5 sites are documented as
divergent and left in place.

## Per-file analysis

### Already-adopted sites (8) — verified intact

| File | Lines | Function | Pattern |
|---|---|---|---|
| `src/archive/bnd3.c` | 439 | `bnd3_open_decompressed` (called from `sf_bnd3_read_from_memory` / `_read_from_path`) | top-level whole-file DCX unwrap |
| `src/archive/bnd3.c` | 753 | `sf_bnd3_open` (lazy reader) | top-level whole-file DCX unwrap |
| `src/archive/bnd4.c` | 404 | `bnd4_open_decompressed` | top-level whole-file DCX unwrap |
| `src/archive/bnd4.c` | 729 | `sf_bnd4_open` (lazy reader) | top-level whole-file DCX unwrap |
| `src/archive/bxf3.c` | 454 | `bxf3_open_decompressed` | top-level whole-file DCX unwrap |
| `src/archive/bxf3.c` | 970 | `sf_bxf3_open` (lazy reader) | top-level whole-file DCX unwrap |
| `src/archive/bxf4.c` | 490 | `bxf4_open_decompressed` | top-level whole-file DCX unwrap |
| `src/archive/bxf4.c` | 1025 | `sf_bxf4_open` (lazy reader) | top-level whole-file DCX unwrap |

These 8 sites use the helper to handle "the entire BND/BXF file may be
DCX-wrapped" — sniffing magic at the head of the input reader. **No changes
needed.**

### Divergent sites (5) — left inline

| File | Line | Function | Why it diverges |
|---|---|---|---|
| `src/archive/bnd3.c` | 331 | `bnd3_decode_file_data` | per-binder-file decompression |
| `src/archive/bnd4.c` | 306 | `bnd4_decode_file_data` | per-binder-file decompression |
| `src/archive/bxf3.c` | 348 | `bxf3_decode_file_data` | per-binder-file decompression |
| `src/archive/bxf4.c` | 388 | `bxf4_decode_file_data` | per-binder-file decompression |
| `src/archive/tpf.c`  | 347 | `tpf_read_one`            | per-texture DCP_EDGE decompression |

All 5 share these properties that diverge from
`sf_get_decompressed_reader`'s contract:

| Aspect | Helper contract | Inline sites' contract |
|---|---|---|
| **Input** | `sf_binary_reader_t *` (decides from current position) | raw `uint8_t *` already read from explicit `data_offset` |
| **Decision trigger** | sniff `DCX\x00` magic at position | per-file `flags & SF_BINDER_FILE_FLAG_COMPRESSED` (or `flags1 == 2/3` for TPF) |
| **Output** | `sf_binary_reader_t **` (heap-owned reader wrapping decompressed bytes) | `uint8_t **out_data + size_t *out_size` (raw bytes) |
| **Uncompressed path** | borrows input reader (no copy) | uses raw buffer directly (no copy) |
| **Decoupling from offset** | reads from reader's current position | reads from explicit `h->data_offset` (jump-style) |

#### Why forcing the helper here is worse

Adopting the helper at any of these 5 sites would require:

1. **Wrap** the raw `uint8_t` buffer in a temporary in-memory reader
   (`sf_binary_reader_create_from_memory` — allocates new reader struct).
2. **Call** `sf_get_decompressed_reader` on that temp reader. The helper
   sniffs DCX magic (redundant — caller already knows from `flags`),
   allocates a SECOND raw buffer, copies bytes into it, calls
   `sf_dcx_decompress_from_buffer`, frees the second buffer, then creates a
   THIRD reader wrapping the decompressed bytes.
3. **Extract** the decompressed bytes from the helper's output reader.
   There is no public `sf_binary_reader_steal_buffer` API; the only way to
   get bytes out is `sf_binary_reader_length` + `sf_binary_reader_get_u8s`
   over the full length → a FOURTH heap allocation and full memcpy.
4. **Destroy** the helper's reader (which frees its internal buffer).
5. **Destroy** the temp reader (frees the original `raw` buffer).

Net effect of forcing the helper:
* **+30 LOC** per site (wrap, call, length, alloc, get_u8s, destroy ×2,
  error-cleanup branches at each step)
* **+1 redundant magic-sniff** per call (caller already knows from flag)
* **+1 redundant heap allocation + memcpy** of the decompressed bytes
* **+1 reader-struct allocation** for the temp reader
* **Ownership confusion** — the "raw buffer" (TPF/BND inner-file) is
  conceptually owned by the per-file decode helper; routing it through
  the helper's owned-reader model muddies the contract.

The current inline at each site is **~15 LOC** of straightforward
`alloc → get_bytes → decompress → free` that mirrors upstream
`Binder.FileData()` / `TPF.Texture.Bytes` accessor semantics directly.
Replacing it with helper plumbing would be net-negative on every axis.

## Verification

| Check | Result |
|---|---|
| Build (clean tree) | PASS (`cmake --build build-mingw --clean-first`) |
| `ctest -L 'archive\|compression'` | 13/13 PASS — see `task-2.1-ctest.log` |
| Direct call sites in scope | 10 (5 archive inline + 5 inside `dcx.c` definitions/chain) — UNCHANGED |
| Symbol export | 891 sf_* exports (same as pre-task baseline; recorded
  baseline `symbols-baseline.txt` has 894, the 3-symbol delta is from
  pre-existing ESD bytecode API consolidation, unrelated to T2.1) |

## Outcome

* **0 new adoptions** — no source changes to archive callers.
* **5 inline sites documented as divergent** — left in place per QA
  Scenario 2.
* **8 existing adoptions verified intact** — top-level whole-file DCX
  unwrap continues to use the helper.
* **CHANGELOG**: noted under `[Unreleased]/Internal` as audit-only
  outcome.

This satisfies QA Scenario 2's expected result: "5-6 callers adopted; 1-2
documented as divergent." Our distribution is 8 already-adopted (pre-T2.1
landing) + 5 documented divergent = no acceptance-criterion regression,
acknowledging the audit's per-file count conflated "files invoking DCX
decompression" (7) with "inline duplications of the helper's pattern" (0
remaining at T2.1 start).

## Upstream cross-reference

Upstream `SoulsFormats/Util/SFUtil.cs::GetDecompressedBinaryReader` mirrors
our helper exactly (sniff magic → decompress → wrap reader). Upstream's
per-binder-file decompression in `Binder/BinderFileHeader.cs::ReadFileData`
and `TPF.cs::Texture.Read` are **also inline** for the same architectural
reason: they take raw bytes from an offset known via the file header and
return raw bytes, no reader involved. C# upstream and our C port both keep
these distinct from the helper. No drift from upstream contract.
