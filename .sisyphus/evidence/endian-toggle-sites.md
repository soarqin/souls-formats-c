# T0.4 — binary_reader Endian-Toggle Sites Audit

## Verdict: PENDING (microbench needed in W4.3 for actual % measurement)

Note: The microbench harness (`tests/microbench/test_flver2_decode_endian.c`) is created
as part of this audit. The actual GO/NO-GO for W4.3 requires running the bench with and
without the fast-path prototype, which happens in W4.3 itself.

## Toggle Site Count: 51 total assignments

### Core infrastructure (not format toggles)
- `src/core/binary_reader.c:56` — constructor initialization (not a toggle)
- `src/core/binary_reader.c:127-128` — `sf_binary_reader_set_big_endian()` implementation
- `src/core/binary_writer.c:78` — writer constructor initialization
- `src/core/binary_writer.c:118` — `sf_binary_writer_set_big_endian()` implementation

### Format-level toggles (mid-stream or at parse entry)
| File:Line | Format | Why we toggle |
|-----------|--------|---------------|
| src/effects/tae.c:131 | TAE | Reset to LE after reading BE header section |
| src/text/fmg.c:105 | FMG | Default LE on create |
| src/text/fmg.c:143-144 | FMG | Set from file header big_endian flag |
| src/text/fmg.c:330 | FMG | DS1 FMG is BE (Demon's Souls version) |
| src/text/fmg.c:702 | FMG | Public setter for big_endian field |
| src/geom/matbin.c:155 | MATBIN | Reset to LE after reading header |
| src/geom/mtd.c:384 | MTD | Reset to LE after reading header |
| src/geom/flver2.c:59 | FLVER2 | Reset to LE (FLVER2 is always LE) |
| src/param/paramdef.c:551 | PARAMDEF | Set from file header |
| src/param/paramdef.c:565 | PARAMDEF | Store big_endian flag in def struct |
| src/param/param.c:268 | PARAM | Set from file header |
| src/param/param.c:281 | PARAM | Store big_endian flag in param struct |
| src/script/emevd.c:72 | EMEVD | Set from file header |
| src/script/emevd.c:96 | EMEVD | Store big_endian flag in emevd struct |
| (+ ~37 more across archive, compression, map modules) | Various | Per-format header detection |

## Mid-Stream Toggle Constraint

**CONFIRMED**: Multiple formats toggle `big_endian` mid-stream (e.g., TAE resets to LE after
reading a BE header section). Type-specialization (separate LE/BE reader types) is **FORBIDDEN**
because it would break these mid-stream toggles.

The fast-path in W4.3 must be limited to:
- `__builtin_expect` branch hint on the `if (r->big_endian)` check
- Stack-local caching of the flag at loop entry where the compiler benefits
- NO type-split, NO removal of `sf_binary_reader_set_big_endian()` public API

## Microbench Harness

Created: `tests/microbench/test_flver2_decode_endian.c`
- Uses synthetic FLVER2 cube (same k_cube_vertices/k_cube_indices as tests/geom/test_flver2_synthetic.c)
- Times sf_flver2_read_from_buffer in a loop (1000 iterations)
- Prints timing to stdout for baseline capture

## Downstream Impact

- **T4.3**: PENDING — run microbench in W4.3 to get actual % delta.
  - If ≥5% win → GO (apply `__builtin_expect` fast-path)
  - If <5% win → NO-GO (skip T4.3)
- Type-specialization is FORBIDDEN regardless of benchmark result.
