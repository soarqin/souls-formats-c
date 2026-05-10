# Phase 4 Issues & Gotchas

## Known Gotchas

- MinGW-w64 POSIX regex (`<regex.h>`) may have compatibility issues — prefer hand-written state machine for PARAMDEF XML Def attribute parsing
- C11 enum defaults to int (4 bytes) — NEVER use sizeof on enum types for assertions
- FormatFlags1/2 must be typedef uint8_t + constants, NOT C enum
- v103 PARAMDEF field size is 0x6C (upstream acknowledges as "wrong") — DO NOT FIX, preserve for round-trip
- FMG byte[0]==0 does NOT guarantee no MD5 — mirror upstream limitation
- EMEVD ER/AC6/Nightreign format: pending Wave 0 probe to confirm if Sekiro alias or Novel
- PARAM apply: endianness mismatch is silently accepted (mirrors upstream bug) — test must verify this
- bit_size==0 in PARAM apply: upstream throws NotImplementedException, we return SF_ERR_BAD_DATA

## Build System Notes

- mxml is already a CPM dependency from Phase 3 (used for XML parsing)
- New source dirs: src/param/, src/text/, src/script/
- New test dirs: tests/param/, tests/script/
- New labels: param, script
- DLL export target: 544-564 (current: 469, adding ~75-95)
