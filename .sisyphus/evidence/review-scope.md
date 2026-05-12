# F4 scope fidelity review

Date: 2026-05-12

## Base note

The requested `main...HEAD` checks are not directly runnable in this checkout because no
local or remote `main` ref exists. The available base ref is `origin/master`; the current
branch is `master` and is `38` commits ahead of `origin/master`. Scope results below use
`origin/master...HEAD` for diff-based checks.

## Required checks

| Check | Result | Status |
|---|---:|---|
| Commit count (`origin/master...HEAD`) | 38 | Recorded |
| Public header stat under `include/souls_formats/` | empty | CLEAN |
| MSB per-subtype field touches | 133 changed non-header diff lines | ISSUE |
| klib/khash outside `src/archive/bhd5.c` | no matches | CLEAN |
| `sf_binary_reader_le_t` / `sf_binary_reader_be_t` specialization | no matches | CLEAN |
| Removed exported `sf_*` symbols | 0 | CLEAN |
| Public header declaration changes excluding `sf_sl2.h` | no matches | CLEAN |

## Additional specified evidence

- Wave 2 T2.1 `sf_get_decompressed_reader` adoption: 10 total matches in `src/`, including 8 archive read adoption sites plus implementation/comment references.
- Wave 3 T3.1 `src/archive/bnd4.c` contains `name_pool` fields and related allocation/free usage.
- Wave 4 T4.1 `src/archive/bhd5.c` contains `#include "khash.h"`, `khash_t(bhd5_lookup)`, and `kh_init(bhd5_lookup)`.
- Wave 4 T4.2 `src/param/paramdef_apply.c` contains `build_field_layout`, `get_field_layout`, and `layout_cache` usage.
- Wave 6 next-batch plan count: 10.

## Verdict

REJECT: required scope checks are clean except the MSB per-subtype field touch count, which is not minimal/scaffolding-only by the required check (`133` changed non-header diff lines).
