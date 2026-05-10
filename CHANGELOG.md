# Changelog

All notable changes to souls-formats-c are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [0.2.0] - 2026-05-10

### Breaking changes

- `sf_oodle_version()` return type changed from `int` to `sf_oodle_version_t` enum.

### New APIs

- `sf_oodle_version_t` enum (SF_OODLE_VERSION_UNKNOWN/6/8/9)
- `sf_oodle_lz_compressor_t` and 7 other OodleLZ_* enums
- `sf_oodle_lz_compress_options_t` struct

### Notes

- Strict upstream alignment policy adopted. See `AGENTS.md §5.x` and
  `docs/api-mapping/UPSTREAM.md` for the pinned SoulsFormatsNEXT commit
  (SHA `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`).
- Tier A row-level mapping for in-scope formats (util + Phase 1-7);
  Tier B inventory-only for legacy/v2+ formats. See `docs/api-mapping/`.

---

## [0.1.0] - 2026-05-10

Initial release covering Phase 0 (scaffolding), Phase 1 (runtime IO,
encoding, math, hash), and Phase 2 (compression + crypto: DCX, AES, Oodle).

**Deliverables:**
- `libsouls_formats.a` / `libsouls_formats.dll` — static + shared library
- `sf_io.h` — binary reader/writer with stream abstraction
- `sf_encoding.h` — Shift-JIS / UTF-16 / UTF-8 converters (Win32)
- `sf_math.h` — POD vec2/3/4, quat, mat4, color types
- `sf_hash.h` — FromPath hash (BHD5 filename hashing)
- `sf_dcx.h` — DCX (de)compression: Zlib, Zstd, Oodle KRAK, DCP/DCX variants
- `sf_oodle.h` — Oodle DLL loader (v6/v8/v9)
- `sf_common.h` — error codes, allocator, SF_API
- 13 Unity test binaries, 13/13 passing
