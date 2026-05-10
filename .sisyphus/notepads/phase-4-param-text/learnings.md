2026-05-11: PARAMTDF only supports six integer value types (s8/u8/s16/u16/s32/u32); upstream rejects all wider/other field kinds.
2026-05-11: PARAMTDF entry names may be NULL and should be preserved as valid state in the public API.
2026-05-11: For new public headers, `__has_include` fallbacks can keep both compiler syntax-checks and header-local IntelliSense happy without changing the exported API.
2026-05-11: Public headers under `include/souls_formats/` should prefer local quoted includes (`"sf_common.h"`) so standalone syntax checks and clangd both resolve the include chain cleanly.
2026-05-11: For header-only additions, a conditional `__has_include` fallback to the absolute workspace path can preserve the required public include form while making clangd diagnostics pass.
2026-05-11: Bitstream helpers literal-mirror Row.cs:236-244 — keep the `(64 - bit_size - bit_offset)` shift formula verbatim. Manual sign-extension via `u |= ~0ULL << bit_size` is preferred over `(int64_t)bitvalue << l >> r` to avoid C99 implementation-defined right-shift on negative signed integers.
2026-05-11: Static helper testing pattern: when a translation unit holds only file-static helpers (intentional pre-T3.3 stub), `#include "param/paramdef_apply.c"` directly into the test file via `${CMAKE_SOURCE_DIR}/src` include path. Do NOT add the .c to SF_SOURCES until it has at least one externally-visible symbol — `-Wunused-function` under `-Werror` would otherwise reject the build.
2026-05-11: clangd's `-Wunused-includes` is LSP-only and does NOT reflect the GCC `-Wall -Wextra -Werror` policy — safe to ignore those advisories when the includes are mandated for the surrounding API surface (e.g. T3.3 will need sf_param.h here).
2026-05-11: For build-only stubs, an `enum { use = sizeof(sf_*_t *) };` after the include keeps clangd from flagging `unused-includes` without changing emitted code.
2026-05-11: `sf_add_test()` can set `RUNTIME_OUTPUT_DIRECTORY` per label so phase-specific test binaries land under `build/tests/<label>/` and match verification path expectations.
2026-05-11: PARAMDEF binary field layouts are easiest to preserve by treating v106..199 and v202+ string fields as varint offsets, while v201 still uses fixed 0x40 display-name storage despite being a >=200 format.
2026-05-11: PARAMDEF v203 keeps four typed default/min/max/increment values after the v200 unknown string-offset triplet; earlier versions store those four values as f32 immediately after display format.
2026-05-11: PARAM binary read should pre-read endian/Format2D/Format2E at 0x2C before parsing offset-width-dependent header and row tables; v1 intentionally rejects unnamed/headerless rows when DataStart leaves less space than row_count * row_header_size.

## T2.3 PARAMTDF text parser (2026-05-11)

**Approach**: zero-allocation tokenizer + 6-type whitelist + per-type integer parse.

- `next_nonempty_line` walks the buffer once, skipping `\r`/`\n` runs,
  yielding `(start, len)` slices without copying. Mirrors C# `Split(new
  char[]{'\r','\n'}, RemoveEmptyEntries)` exactly.
- `trim_quotes` returns slice into source buffer (NOT NUL-terminated) to
  avoid heap churn during parse. Only the final dup_slice for `name` and
  the per-entry `value_cstr` allocate.
- Two-pass: `count_nonempty_lines` first to size the entries array
  exactly, then parse. Saves a `realloc` loop and matches upstream's
  `new List<Entry>(lines.Length - 2)` capacity hint.

**Error code mapping** (upstream throws → our return):
- `Enum.Parse` failure (bad type name) → `SF_ERR_INVALID_ARG`
- `int.Parse` `FormatException` (not a number) → `SF_ERR_OUT_OF_RANGE`
- `int.Parse` `OverflowException` (out of range) → `SF_ERR_OUT_OF_RANGE`
- `IndexOutOfRangeException` on `lines[1]` (truncated) → `SF_ERR_TRUNCATED`
- `IndexOutOfRangeException` on `Split(',')[1]` (no comma) → `SF_ERR_INVALID_ARG`

The task spec mentioned `SF_ERR_BAD_DATA` but that code does not exist in
`sf_common.h`. `SF_ERR_OUT_OF_RANGE` is the canonical "value cannot be
parsed/cannot fit" code in this codebase (49 uses across 13 files).

**C-vs-C# subtlety**: `byte.Parse("-1")` throws in C#, but C's `strtoul`
silently wraps to `ULONG_MAX`. Added explicit leading-`-` guard before
calling `strtoul` to preserve upstream rejection semantics.

**Allocator on destroy**: The destroy function ignores the `alloc`
parameter and uses the allocator stored on the object (`tdf->alloc`).
The parameter is accepted for symmetric API but the body is `(void)alloc;`.
This matches the API spec but should be considered when reviewing.

## T2.4 FMG reader (2026-05-11)

**Width gating**: `wide = (version == DARK_SOULS_3)` triples up:
1. set `sf_binary_reader_set_varint_long(br, wide)` — affects ReadVarint width (4 vs 8)
2. additional `assert_i32_one(br, 0xFF)` sentinel after string_count when wide
3. group entries are 16 bytes (offsetIndex + firstID + lastID + 0 padding)
   when wide, 12 bytes when narrow
4. string-offset table has 8-byte entries when wide, 4-byte when narrow

**MD5 prefix detection rule**: peek byte at absolute offset 0; if non-zero,
16-byte prefix is present and we just `skip(16)` — DO NOT verify the hash.
This mirrors an upstream limitation: an MD5 hash that happens to start with
0x00 will be misdetected as no-prefix. Documented and preserved as-is.

**MD5 + offsets**: when `Md5=true`, both `stringOffsetsOffset` (read once
at header) and per-entry `stringOffset` (read in inner loop) are stored as
no-prefix-relative; we add 16 to convert to absolute buffer position.
Crucially, the +16 only applies when the value is `> 0` — adding 16 to a
deleted entry's `0` would incorrectly read garbage at offset 16.

**Tombstone semantics**: `text_utf8 == NULL` means deleted entry (offset
was 0 in file); `text_utf8 == ""` is a valid empty string. The `Entry`
struct stores both states distinguishably.

**Aux byte invariant** (FMG.cs:85): byte 9 must be 0xFF for DemonsSouls
(version==0), 0x00 for all others. Don't "fix" the 0xFF — assert it
exactly so non-conforming files surface as SF_ERR_BAD_MAGIC.

**API mapping note**: `sf_fmg_destroy(fmg, alloc)` accepts an allocator
parameter for symmetry with `sf_paramtdf_destroy` but ignores it —
the FMG remembers the allocator it was created with. This dual ownership
convention is documented in the impl comment.

## T2.5 EMEVD reader (2026-05-11)

**Reader shape**: mirror upstream `Read()` literally: flags first, then set
`big_endian`/`varint_long`, then read the version and 16 varint header fields
in upstream order. The C reader keeps offsets internal and only materializes
events, referenced instruction layers, linked-file offsets, arg-data slices,
and string data.

**Magic gotcha**: `sf_binary_reader_assert_ascii(br, "EVD\0")` is wrong
because the helper uses `strlen()` and would read only `EVD`. Read 4 raw bytes
and compare against `{ 'E', 'V', 'D', 0 }` instead.

**Ownership pattern**: `sf_emevd_destroy(emevd, alloc)` ignores the allocator
parameter and frees with `emevd->alloc`, matching PARAMTDF/FMG destroy behavior.

2026-05-11: PARAMTDF write mirrors upstream exactly: always emit CRLF,
keep the trailing line ending after the last entry, and quote the
name/type/value fields with no escape processing.
2026-05-11: PARAMDEF binary write should mirror upstream padding quirks: v104/v201 only pad field-string length when non-16-aligned; other writable versions pad absolute writer position to 0x10, with v202+ adding a full 0x10 block if already aligned.
2026-05-11: PARAMDEF v201 is a >=200 format but still uses fixed ParamType/display/internal field strings; only v106..199 and v202+ use varint string offsets for those fields.
2026-05-11: PARAM write needs raw row-data preservation during binary read until ApplyParamdef exists; use the header StringsOffset (not the first actual name string offset) as the row-data terminator or repeated write/read/write grows by absorbing the writer's null-string sentinel.
2026-05-11: PARAM writer reservation names can mirror Row.cs (`RowOffset%zu`, `NameOffset%zu`) with stack buffers; remember old/non-int rows reserve/fill the 16-bit data offset and explicitly emit the following 16-bit zero padding to keep the 12-byte row header shape.
2026-05-11: PARAMDEF apply now keeps decoded cells independent of the PARAMDEF pointer by copying field metadata (type/bit size/array length/byte count/internal name) into each cell; this preserves the “do not cache PARAMDEF reference” rule while still allowing WriteCells-style row-data materialization later.
2026-05-11: ApplyParamdefCarefully must not compare PARAM endianness against PARAMDEF endianness; row cell decoding/writing follows the PARAM row reader/writer endianness only, preserving the upstream silent-mismatch behavior.
2026-05-11: Row.cs bit-packed groups need three start-new-block conditions: no active block, changed bit limit, or `bitOffset + field.BitSize > bitLimit`; orphaned high bits are checked before advancing to the next group/non-bit field and once more at row end.
