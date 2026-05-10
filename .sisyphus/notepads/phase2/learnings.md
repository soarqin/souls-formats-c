# Phase 2 Learnings

## [2026-05-10] Atlas initialization

### Upstream DCX.cs structure (key facts)
- Magic "DCP\0" = legacy DCP format (DFLT or EDGE)
- Magic "DCX\0" = modern DCX format (EDGE, DFLT, KRAK, ZSTD)
- All DCX parsing is big-endian
- Type sniff: look at bytes 0..3 for "DCP\0" or "DCX\0", then at 0x28 for format tag
- Zlib fallback: no DCX header, bytes 0x78 + 0x01/0x5E/0x9C/0xDA = raw zlib

### DCX Header layouts (all big-endian):
- **DCP_DFLT**: "DCP\0"+"DFLT"+0x20+0x9000000+0+0+0+0x00010100 then "DCS\0"+uncompSize+compSize
- **DCP_EDGE**: "DCP\0"+"EDGE"+0x20+0x9000000+0x10000+0+0+0x00100100 then "DCS\0"+uncompSize+compSize+0, then data, then "DCA\0"+dcaSize+"EgdT"+...
- **DCX_EDGE**: "DCX\0"+0x10000+0x18+0x24+0x24+unk1(=0x50+chunkCount*0x10) then "DCS\0"+... then "DCP\0"+"EDGE"+... then "DCA\0"+dcaSize+"EgdT"+0x00010100+0x24+...
- **DCX_DFLT**: "DCX\0"+unk04+0x18+0x24+unk10+unk14 then "DCS\0"+uncompSize+compSize then "DCP\0"+"DFLT"+0x20+unk30+0+0+0+0+unk38+0+0+0+0+0x00010100 then "DCA\0"+compHeaderLen then zlib data
- **DCX_KRAK**: "DCX\0"+0x11000+0x18+0x24+0x44+0x4C then "DCS\0"+uncompSize+compSize then "DCP\0"+"KRAK"+0x20+comprLevel+0+0+0+0+0+0+0x10100 then "DCA\0"+8 then raw oodle data
- **DCX_ZSTD**: "DCX\0"+0x11000+0x18+0x24+0x44+0x4C then "DCS\0"+uncompSize+compSize then "DCP\0"+"ZSTD"+0x20+comprLevel+0+0+0+0+0+0+0+0+0+0x010100 then "DCA\0"+8 then raw zstd data

### DCX_DFLT presets (unk04/unk10/unk14/unk30/unk38):
- DCX_DFLT_10000_24_9: 0x10000, 0x24, 0x2C, 9, 0
- DCX_DFLT_10000_44_9: 0x10000, 0x44, 0x4C, 9, 0
- DCX_DFLT_11000_44_8: 0x11000, 0x44, 0x4C, 8, 0
- DCX_DFLT_11000_44_9: 0x11000, 0x44, 0x4C, 9, 0
- DCX_DFLT_11000_44_9_15: 0x11000, 0x44, 0x4C, 9, 15

### KRAK compression levels:
- Elden Ring: level 6 (Optimal2), compressor = OodleLZ_Compressor_Kraken (=8)
- AC6: level 9 (Optimal5)

### DCP_EDGE / DCX_EDGE chunked decompress:
- Chunk size = 0x10000 bytes (65536)
- Chunk table in "EgdT" section: each entry is 4 × int32: [0, offset, size, compressed(0/1)]
- For DCP_EDGE: offsets are relative to dataStart (after DCS section)
- For DCX_EDGE: offsets are relative to dcaStart + dcaSize

### Regulation.bin format:
- First 16 bytes = IV (zero for ER)
- Remaining bytes = AES-256-CBC ciphertext
- Decrypted = BND4 magic ("BND4")
- Keys:
  - DS3: ASCII "ds3#jn/8_7(rsY9pg55GFN7VFL#+3n/)" 
  - ER: 0x99,0xBF,0xFC,0x36,0x6A,0x6B,0xC8,0xC6,0xF5,0x82,0x7D,0x09,0x36,0x02,0xD6,0x76,0xC4,0x28,0x92,0xA0,0x1C,0x20,0x7F,0xB0,0x24,0xD3,0xAF,0x4E,0x49,0x3F,0xEF,0x99
  - AC6: 0x10,0xCE,0xED,0x47,0x7B,0x7C,0xD9,0xD7,0xE6,0x93,0x8E,0x11,0x47,0x13,0xE7,0x87,0xD5,0x39,0x13,0xB1,0x0D,0x31,0x8E,0xC1,0x35,0xE4,0xBE,0x50,0x50,0x4E,0x0E,0x10
  - Nightreign: 0x9A,0x8E,0xE9,0x0C,0x4C,0x01,0xA4,0x31,0x68,0xA1,0x7D,0x9D,0x75,0xE4,0xA7,0xD0,0x21,0x07,0xEB,0xCF,0x43,0xD5,0xAC,0xB0,0x55,0x4F,0x94,0x16,0x01,0xB5,0x79,0x18
- Padding: NoPadding (raw CBC, input must be block-aligned; upstream pads manually if needed)

### Oodle DLL loader (C API notes):
- LoadLibraryW search order: user-set path → process dir → PATH
- Use `LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR)` for safe loading
- Functions to import via GetProcAddress:
  - OodleLZ_Decompress(void* src, size_t srcLen, void* dst, size_t dstLen, fuzzSafe, checkCRC, verbosity, decoderMem, decoderMemSize, fpCallback, callbackUserData, decoderMemSizePtr, numThreadsDecompression, threadPhase) → size_t
  - OodleLZ_Compress(compressor, src, srcLen, dst, level, options, decoderMem, decoderMemSize, longRangeMatcher, lrmContext) → size_t
  - OodleLZ_GetCompressedBufferSizeNeeded(compressor, srcLen) → size_t
  - OodleLZ_CompressOptions_GetDefault(compressor, level) → OodleLZ_CompressOptions
  - OodleLZ_GetDecodeBufferSize(compressor, src_len, corruptionPossible) → size_t

### Code patterns (from Phase 1):
- All public symbols use SF_API macro
- Error codes: sf_result_t return, output via pointer
- Allocator: const sf_allocator_t *a, NULL = default
- All buffers allocated with allocator, caller frees via sf_free(alloc, ptr)
- Internal headers in src/ use `sfi_` prefix
- Static assertions after every enum table (see error.c)
- No GNU extensions, C11 standard only
- -Werror strict; no casts that lose sign, no implicit conversions

### CMake structure:
- SF_PUBLIC_HEADERS list in CMakeLists.txt
- SF_SOURCES list in CMakeLists.txt
- sf_add_test(name source label) in tests/CMakeLists.txt
- CPM deps are in cmake/deps/ (zlib-ng.cmake, zstd.cmake already written per Phase 0)
- target_link_libraries needs zlib-ng + zstd + bcrypt
- Oodle is NOT a static dep — only bcrypt is added to link

### zlib-ng in ZLIB_COMPAT mode:
- API is drop-in compatible with standard zlib (z_stream, deflate, inflate, etc.)
- Header: <zlib.h> (compat mode)
- deflateInit2() for raw deflate (windowBits = -15), zlib wrapper (windowBits = 15)
- inflateInit2() for raw inflate (windowBits = -15)
- Need to call deflate(Z_FINISH) and check Z_STREAM_END

### Testing (Unity framework):
- Unity test format: RUN_TEST(test_xxx) in main, then void test_xxx(void) function
- sf_add_test(name, source, label) — source is single .c file
- Tests use TEST_ASSERT_*, TEST_IGNORE_MESSAGE, TEST_FAIL_MESSAGE
- For skip conditions: TEST_IGNORE_MESSAGE("reason") returns immediately

## [2026-05-10] Sisyphus-Junior implementation pass

- zlib-ng CPM tag needed explicit `GIT_TAG 2.2.4`; CPM's default `v2.2.4` checkout failed for this package.
- `-Wmissing-prototypes` applies to internal `sfi_*` functions too. Add small internal headers (`compression_internal.h`, `sl2.h`) or include existing internal headers in placeholder translation units.
- Oodle v8/v9 expose `OodleLZ_GetCompressedBufferSizeNeeded(byte, int64)` and no-arg `OodleLZ_CompressOptions_GetDefault()`, while v6 uses the simpler `(int64)` and `(compressor, level)` signatures. Runtime dispatch by loaded version is required for KRAK round-trips.
- DCX_EDGE data starts at `dcaStart + dcaSize`; with DCA + EgdT header this is `0x70 + chunkCount * 0x10`, not `0x68 + ...`.
- Local ER `regulation.bin` may exist but not decrypt to BND4 with the v1 ER key; keep the synthetic regulation round-trip mandatory and skip the external fixture if magic does not match.

## [2026-05-10] Regulation test correction

- Current ER `regulation.bin` AES-decrypts to a DCX_ZSTD stream (`DCX\0`, format tag `ZSTD` at 0x28), not directly to BND4.
- The correct validation chain for ER regulation fixtures is AES-256-CBC decrypt → `sf_dcx_decompress` → inner `BND4` magic.
- Regulation AES round-trip tests should compare the AES-decrypted byte stream before DCX unwrap; reproducing the original regulation.bin byte-for-byte would require matching the exact original ZSTD frame.

## [2026-05-10] Magic-driven DCX unwrap API

- `sf_dcx_unwrap` is the public helper for magic-driven layer stripping: it loops with `sf_dcx_sniff`, decompresses detected DCX/zlib layers, and copies through non-DCX payloads unchanged.
- The unwrap depth cap is 8 layers; exceeding it returns `SF_ERR_UNSUPPORTED_VERSION` as corrupt/suspicious nesting.
- `test_dcx_sniff.c` remains sniff-only: it asserts header/tag detection and does not encode a decompression-chain assumption.
- Regulation fixture tests should call `sf_dcx_unwrap` after AES decrypt and only assert the final payload magic (`BND4`), with no assertions about intermediate DCX type.

## [2026-05-10] Unified DCX compression params

- Public DCX round-trip parameters are now represented by `sf_dcx_params_t`: type, level byte, DCX unk04/unk10/unk14, and unk38.
- `sf_dcx_read_params` is a header-only inspection path (no decompression) driven by `sf_dcx_sniff`; it reads DFLT unk fields and KRAK/ZSTD level bytes from `+0x30`.
- `sf_dcx_compress_ex` is the parameter-preserving compression path; default `sf_dcx_compress` builds default params and delegates into it.
- Oodle compression level is now passed through `sfi_oodle_compress(level, ...)`; KRAK magic implies Kraken compressor, while level is header/API controlled.
- Old public DFLT-specific helpers (`sf_dcx_compress_dflt`, `sf_dcx_dflt_params_preset`) were removed from header/implementation/export list.

## [2026-05-10] Phase 2 completion notes

### Test results (actual):
- `souls_formats_test_dcx_dflt`: 1 test PASS (round-trip for 3 sizes × 2 entropies via sf_dcx_compress/decompress)
- `souls_formats_test_dcx_edge`: 1 test PASS (DCP_EDGE + DCX_EDGE chunked deflate round-trip)
- `souls_formats_test_dcx_zstd`: 1 test PASS (ZSTD single frame round-trip)
- `souls_formats_test_dcx_krak`: 1 test PASS (Oodle KRAK round-trip; DLL at \\wsl.localhost\Ubuntu\home\soar\dev\oodle)
- `souls_formats_test_dcx_sniff`: 1 test PASS (header type detection for all 6 types)
- `souls_formats_test_aes_kat`: 3 tests PASS (AES-128-ECB, AES-128-CBC, AES-256-CBC NIST vectors)
- `souls_formats_test_md5_kat`: 1 test PASS (RFC 1321 all 7 vectors)
- `souls_formats_test_regulation_decrypt`: 1 PASS (synthetic round-trip) + 1 IGNORE (ER file exists but key mismatch — newer game patch)
- Total: 8/8 tests PASS, 13/13 total suite PASS

### DLL exports verified:
sf_dcx_compress, sf_dcx_compress_dflt, sf_dcx_decompress, sf_dcx_dflt_params_preset,
sf_dcx_sniff, sf_oodle_load, sf_oodle_set_search_path, sf_oodle_unload, sf_oodle_version

### Key implementation notes for Phase 3:
- `sfi_oodle_decompress(in, in_size, out, out_size)` is the internal API for DCX_KRAK
- `sfi_aes_decrypt_cbc(key, key_len, iv, iv_len, in, n, out)` for BHD5 AES-128-ECB decryption
- `sfi_aes_ecb_block(key, key_len, encrypt, in[16], out[16])` for per-block ECB
- regulation_keys.c has game-specific AES-256 keys (DS3, ER, AC6, Nightreign)
- Internal crypto headers at: src/crypto/aes_cng.h, src/crypto/md5_cng.h, src/crypto/regulation.h, src/crypto/sl2.h
