# Phase 3 Decisions

## Locked Decisions (from Plan Interview + Momus Review)

| ID | Decision |
|---|---|
| D1 | BND/BXF: Phase 3 ships eager + streaming Reader for ALL 4 formats |
| D2 | BHD5 game key coverage: v1 = 4 games (Sekiro/ER/Nightreign/AC6) |
| D3 | TPF Headerizer capped to PC-only; non-PC → SF_ERR_UNSUPPORTED_VERSION |
| D4 | Synthetic tests inline with each format task; ER e2e as separate tasks |
| GAP-1 | BHD5 RSA decrypt integrated with embedded community PEM public keys |
| GAP-2 | BHD5 game enum extended to 4 v1-target values |
| GAP-9/10 | PC-only TPF + minimal sfi_dds_parse_header (~50 LOC) |

## API Design Decisions

- `sf_binder_format_t` and `sf_binder_file_flags_t` are `typedef uint8_t`, NOT enum
  - Reason: C11 enum defaults to int; cannot reliably be 1 byte across MSVC+clang-cl+MinGW
  - Use `#define` constants with `_Static_assert` per bit to catch drift

- `sf_binder_file_t.id` is `int32_t` (sentinel -1), NOT `int64_t`
  - Mirrors upstream Int32; Int64 would corrupt real files

- `sf_binder_file_t.compression_info` is `sf_dcx_compression_info_t` (preserves preset)
  - NOT `sf_dcx_type_t` which would lose preset info needed for round-trip

- Round-trip semantics:
  - Synthetic fixtures: BYTE-EQUAL
  - Real ER e2e files: CONTENT-EQUAL only (FromSoft hash tables are non-deterministic)

- T1 is self-contained: includes BOTH header decls AND implementations of 10 public helpers
  - 8 has-* helpers + 2 timestamp helpers
  - T2 is internal-only (adds sfi_* helpers, no new DLL exports)

## T2 — binder_common.c

* **Added bit_big_endian parameter to `sfi_binder_read_format` and
  `sfi_binder_write_format`** despite the task spec listing them without
  it. Justification: AGENTS.md §5.x mandates strict upstream parity, and
  upstream `Binder.ReadFormat(br, bool bitBigEndian)` requires it for
  correct semantics; the heuristic-only path is not bijective on the
  full 256-byte range. Test 1 in the PLAN ("roundtrip 256 with
  BitBigEndian=false AND true") cannot pass without this param.

* **`sfi_binder_file_header_t.compression_info` defaults to
  `SF_DCX_TYPE_ZLIB`** matching upstream `Compression = new
  DCX.ZlibCompressionInfo()` in BinderFileHeader.cs:65.

* **Hash table bucket-sort uses qsort on a separate `ordered[]` array**
  instead of in-place sort: each entry must end up at its bucket's
  contiguous slot in upstream-required order. Bucket-major-then-hash-asc
  ordering is built in two passes (insertion via cursor, then qsort per
  bucket). O(n log n) overall, matches upstream output byte-for-byte.

* **bit_big_endian for BND3/BND4 read/write file header passed through
  explicitly** rather than recovered from the format byte's BigEndian
  bit, because the BND file format stores them as separate fields and
  they can disagree (some Sekiro BND4s have BigEndian=false but
  bit_big_endian=true).

## T10 — BHD5 reader shape

* **BHD5 opens the BHD index eagerly but keeps BDT file-backed**. The handle owns an
  `sf_istream_t` for the paired BDT and `sf_bhd5_extract_*` reads only the requested
  `padded_size` slice before applying inline AES ranges in-place.

* **v1 game enum selects RSA key only**. Sekiro, Elden Ring, Nightreign, and AC6 all
  use the same ER+/DS3-style 64-bit file-header path in this implementation; the game
  value is still required for encrypted `.bhd` RSA unwrap key selection.

* **SHA hashes remain opaque metadata**. The reader stores the 32-byte hash blob when
  present but never verifies or recomputes it; `sf_bhd5_write` re-emits parsed metadata.
