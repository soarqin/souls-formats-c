# T0.6 — Per-Entry vs Per-Archive Alloc-Site Audit

## Verdict: GO for T3.1-T3.5 (all confirmed per-entry hot paths)

## Top-15 Alloc Sites Ranked by entries × cost

| Rank | File:Line | Classification | Typical N | Remedy | Est. reduction |
|------|-----------|----------------|-----------|--------|----------------|
| 1 | src/archive/bhd5.c (12 sites) | Per-entry inside loop | ~25,000 (ER Data0) | bulk-alloc entries + name pool | ~24,999 allocs/bhd |
| 2 | src/effects/fxr3_xml_read.c (23 sites) | Per-element nested | ~50-500 nodes | scratch-buffer for transient strings | ~40% reduction |
| 3 | src/archive/bnd4.c:366,771 | Per-entry inside loop | ~5-50 | bulk-alloc name array | N-1 allocs/bnd |
| 4 | src/archive/bxf4.c (15 sites) | Per-entry inside loop | ~5-50 | bulk-alloc name array (mirror BND4) | N-1 allocs/bxf |
| 5 | src/archive/bnd3.c (15 sites) | Per-entry inside loop | ~5-50 | bulk-alloc name array | N-1 allocs/bnd |
| 6 | src/archive/bxf3.c (16 sites) | Per-entry inside loop | ~5-50 | bulk-alloc name array (mirror BND3) | N-1 allocs/bxf |
| 7 | src/param/param.c (9 sites) | Per-row inside loop | ~100-10,000 | contiguous arena per param | N-1 allocs/param |
| 8 | src/effects/tae.c (8 sites) | Per-animation | ~100-1000 | bulk-alloc-with-count | moderate |
| 9 | src/core/encoding_win32.c (14 sites) | Per-call (transient) | 1 | scratch-buffer | minimal |
| 10 | src/archive/tpf.c (10 sites) | Per-entry inside loop | ~5-100 | bulk-alloc name array | N-1 allocs/tpf |
| 11 | src/geom/flver2_decode.c (8 sites) | Per-mesh/vertex | ~100-10,000 | bulk-alloc-with-count | significant |
| 12 | src/script/esd_bytecode.c (8 sites) | Per-state | ~50-500 | bulk-alloc-with-count | moderate |
| 13 | src/param/paramdef.c (12 sites) | Per-field | ~50-200 | bulk-alloc-with-count | moderate |
| 14 | src/geom/matbin.c (7 sites) | Per-param | ~10-50 | bulk-alloc-with-count | minor |
| 15 | src/compression/dcx.c (9 sites) | Per-archive (1 call) | 1 | no-op | none |

## Per-File Action List for Wave 3

### T3.1: BND3/BND4 name-pool (GO)
- `src/archive/bnd4.c:366` — `sf_strdup(b->alloc, headers[i].name_utf8)` in loop
- `src/archive/bnd4.c:771` — same pattern on write path
- `src/archive/bnd3.c` — analogous pattern
- Action: compute Σ strlen first, single bulk alloc, copy names into block
- Expected: N-1 allocs per archive (N = entry count)

### T3.2: BXF3/BXF4 name-pool (GO — mirrors T3.1)
- `src/archive/bxf4.c` — same per-entry strdup pattern
- `src/archive/bxf3.c` — same
- Action: same as T3.1

### T3.3: BHD5 bulk alloc + name pool (GO — highest priority)
- `src/archive/bhd5.c` — 12 alloc sites, ~25K entries in ER Data0
- Action: single bulk alloc for entry array + single name pool
- Expected: ~24,999 fewer allocs per Data0.bhd parse

### T3.4: FXR3 XML scratch buffer (GO)
- `src/effects/fxr3_xml_read.c` — 23 alloc sites, many transient
- Action: per-parse scratch arena for transient strings
- Expected: ~40% reduction in allocator calls per parse

### T3.5: PARAM row arena (GO — conditional on row count)
- `src/param/param.c` — per-row alloc in read loop
- Action: single bulk alloc for all row data
- Expected: N-1 allocs per PARAM (profitable for ≥1000 rows)
- Note: only apply to PARAMs with ≥1000 rows (SpEffectParam, EquipParamWeapon, etc.)

## Notes

- `src/compression/dcx.c` (9 sites): all per-archive (1 call per DCX), not per-entry → no-op
- `src/core/encoding_win32.c` (14 sites): per-call transient Win32 bridge → scratch-buffer
  possible but low priority (not in hot loop)
- `src/effects/tae.c` (8 sites): per-animation, not per-frame → moderate priority
