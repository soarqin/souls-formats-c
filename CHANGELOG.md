# Changelog

All notable changes to souls-formats-c are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Internal
- Reserve/fill audit (T0.2): 247 reserve calls vs 238 fill calls — all mismatches confirmed Legit (dynamic name patterns via snprintf/helper functions). No bugs found.
- Magic-check helper audit (T2.2): sampled 20 diverse `SF_ERR_BAD_MAGIC` sites; only 7/20 matched the proposed return-on-fail `assert_ascii` pattern, so `SF_ASSERT_MAGIC` extraction was skipped.
- `SF_ENABLE_PHASE7` was never landed as a CMake option; Phase 7 (TAE/FXR3) is permanently compiled in since v0.4.0. The option reference in v0.4.0 changelog was erroneous and has been removed.
- `sf_get_decompressed_reader` adoption audit (T2.1): of the 7 files invoking DCX decompression, 8 top-level read paths in `bnd3/bnd4/bxf3/bxf4` already use the helper (since initial Phase 3 port). The remaining 5 inline `sf_dcx_decompress_from_buffer` callsites — 4 per-binder-file decoders (`bnd3/bnd4/bxf3/bxf4`) plus 1 per-texture DCP_EDGE decoder (`tpf`) — operate on raw bytes/explicit offset/flag-driven semantics that diverge from the helper's reader/sniff/wrap contract; forcing adoption would add ~30 LOC/site plus a redundant heap copy. Documented as divergent per QA Scenario 2; no source changes. Evidence: `.sisyphus/evidence/task-2.1-skipped-callers.md`, `task-2.1-ctest.log`.

---

## [0.4.0] - 2026-05-12

### Added
- TAE format support (SDT version 0x1000D; covers Sekiro + Elden Ring)
- FXR3 format support (DS3 version 4 + Sekiro version 5; binary + XML round-trip)
- 2 new public headers: `sf_tae.h`, `sf_fxr3.h`
- 5 new test binaries under labels `anim` + `e2e_er`
- `libsouls_formats.dll` now copied to each test output directory (fixes DLL-not-found on Windows)

### Notes
- TAE Template subsystem deferred to v1.2 (ParameterContainer remains opaque bytes)
- Non-SDT TAE formats (DS1/SOTFS/DS3/BB/DES/DESR) deferred to v2 (legacy games)
- FXR3 XML round-trip uses structural equality (not byte-equal) due to mxml whitespace

## [0.3.0] — 2026-05-10

### Added
- Archive container layer: 7 formats (BND3/BND4/BXF3/BXF4/BHD5/TPF/ENFL) — full read/write + streaming readers
- BHD5: RSA-decrypt for Data0.bhd (Sekiro/ER/Nightreign/AC6 community PEM keys); AES range decryption inline from .bhd data
- Phase 1 retro-fit: `sf_reverse_bits_u8` for binder format byte read/write
- Phase 1 retro-fit: `sf_path_hash_64` for BHD5 ER+ 64-bit hash
- Phase 2 retro-fit: `sfi_aes_decrypt_ecb_buffer` for BHD5 range decryption
- Minimal DDS header parser (`sfi_dds_parse_header`) for TPF texture metadata
- ER e2e test helper (`er_extract_from_data0`) — singleton opening real ER Data0.bhd/bdt
- CLI example: `sf_bnd_extract.exe` — extract BND4 archives to disk

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
- `sf_get_decompressed_reader()` (declared in `sf_io.h`, defined in
  `src/core/sf_util.c`) — mirrors upstream `SFUtil.GetDecompressedBinaryReader`.
  Returns the input reader unchanged when not DCX (borrow path), or a new
  reader over the decompressed buffer when DCX (owned path). Detection is
  strict-upstream `DCX.Is` semantics (`DCP\0` / `DCX\0` magic only).
- `sf_binary_reader_create_from_memory()` — mirrors upstream
  `new BinaryReaderEx(big_endian, byte[])`. Takes ownership of the supplied
  heap buffer; a single `sf_binary_reader_destroy()` frees the buffer and
  closes the internal memory istream.

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
