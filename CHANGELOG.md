# Changelog

All notable changes to souls-formats-c are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [0.2.0] - 2026-05-10

### Breaking changes

- `sf_dcx_params_t` and `sf_dcx_read_params()` were removed; use
  `sf_dcx_compression_info_t` plus the DCX preset factory helpers instead.
- `sf_oodle_version()` return type changed from `int` to `sf_oodle_version_t` enum.
- `sf_binary_writer_pattern()` was renamed to `sf_binary_writer_write_pattern()` to match
  upstream `BinaryWriterEx.WritePattern`; no compatibility alias is provided.
- `sf_binary_reader_read_vec3_11_11_10()` renamed to
  `sf_binary_reader_read_11_11_10_vec3()` to mirror upstream
  `Read11_11_10Vector3`.
- `sf_binary_reader_assert_u8/u16/u32/i32/u64()` now use multi-option
  `(size_t n_options, const T *options, T *out_value)` signatures; use the
  new `_one()` helpers for the previous single-option convenience form.
- The internal `sfi_regulation_decrypt`/`sfi_regulation_encrypt` helpers and
  the `sfi_regulation_game_t` enum (`SFI_REGULATION_DS3` / `_ER` / `_AC6` /
  `_NIGHTREIGN`) were removed. Use the new public
  `sf_regulation_{decrypt,encrypt}` API and `sf_regulation_key_t` enum from
  `<souls_formats/sf_regulation.h>` instead.

### New APIs

- `sf_dcx_compression_info_t` tagged union with all 9 upstream `CompressionInfo`
  variants, plus `sf_dcx_default_type_t`, `sf_dcx_dflt_compression_preset_t`, and
  `sf_dcx_krak_compression_preset_t`.
- DCX factory helpers: `sf_dcx_compression_info_from_default_type()`,
  `sf_dcx_compression_info_from_dflt_preset()`, and
  `sf_dcx_compression_info_from_krak_preset()`.
- DCX overloads mirroring upstream `Is`, `Decompress`, and `Compress` entry points:
  `sf_dcx_is_from_buffer/stream/path()`,
  `sf_dcx_decompress_from_buffer/stream/path()`, and
  `sf_dcx_compress_to_buffer/stream/path()`.
- `sf_oodle_version_t` enum (SF_OODLE_VERSION_UNKNOWN/6/8/9)
- `sf_oodle_lz_compressor_t` and 7 other OodleLZ_* enums
- `sf_oodle_lz_compress_options_t` struct
- `sf_binary_writer_reserve_*` / `sf_binary_writer_fill_*` pairs for all 12 primitive
  writer types, including bool/i8/u8/i16/u16/f32/f64.
- BinaryReaderEx-aligned reader APIs: plural primitive reads, full primitive
  `Get*`/plural `Get*` coverage, `GetASCII`/`GetShiftJIS`/`GetUTF16`,
  multi-option asserts for every primitive/varint, enum validation reads,
  per-reader flexible assertions plus default setter, and borrowed stream
  accessor.
- `sf_binary_writer_write_*s` plural primitive writes for bool/i8/u8/i16/u16/i32/u32/
  i64/u64/f32/f64/varint arrays.
- `sf_binary_writer_pad_ff()`, `sf_binary_writer_to_array()`,
  `sf_binary_writer_finish_bytes()`, and `sf_binary_writer_stream()`.
- `sf_regulation.h` public header exposing the AES-256-CBC byte-level
  RegulationDecryptor API: `sf_regulation_key_t` enum (DS3/ER/AC6/ER:NR),
  generic `sf_regulation_decrypt()` / `sf_regulation_encrypt()`, and eight
  game-specific wrappers (`sf_regulation_{decrypt,encrypt}_{ds3,er,ac6,ernr}`).
  `sf_regulation_encrypt_ernr()` faithfully mirrors upstream's
  `EncryptERNRRegulation` quirk and uses the EldenRing key, not the Nightreign
  key — see `docs/api-mapping/util-cryptography-regulation-decryptor.md`.

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
