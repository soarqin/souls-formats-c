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
