# Phase 2 Decisions

## [2026-05-10] Atlas initialization

### Crypto API decision: internal-only for v1
- AES and MD5 are NOT exposed in the public API for v1
- Only `sf_dcx_*` and `sf_oodle_*` are public
- Crypto functions live in `src/crypto/` with internal header `aes_cng.h` / `md5_cng.h`
- Uses `sfi_` prefix per internal convention

### Oodle loader design:
- Global module state (loaded DLL handle + function pointers)
- Lazy loading on first use (or explicit sf_oodle_load())
- "tried but failed" flag to avoid hammer retries
- sf_oodle_unload() for tests — allows testing SF_ERR_OODLE_NOT_FOUND path
- Single vtable (struct with function pointers) — no per-version dispatch needed since all 3 versions have identical ABI for the 5 functions we use

### zlib-ng vs zstd in CMake:
- Both already have CPM recipe files in cmake/deps/
- Need to include() them and add to target_link_libraries
- zlib-ng in ZLIB_COMPAT mode → link against "zlib" target

### DCP_DFLT preset for compression:
- Use DCX_DFLT_11000_44_9 as default for write (matches DS3/ER common files)
- User can pass specific unk values for round-trip accuracy

### Pad alignment for DCX data:
- DCX_KRAK: pad compressed data to 0x10 alignment after writing
- DCX_EDGE chunks: each chunk padded to 0x10 alignment  

### SL2 (save file):
- AES-128-CBC with game-specific key
- MD5 hash stored in header for integrity check
- Not exposed publicly in v1 — internal implementation, used by future save parsing

## [2026-05-10] Sisyphus-Junior implementation pass

- EDGE compression writes raw (uncompressed) 0x10000-byte chunks with valid EgdT entries. Decompression supports both raw and raw-deflated chunks, so round-trips and reader compatibility are covered without forcing chunk deflate when it is not smaller.
- Added aggregate CMake targets `souls_formats_test_compression` and `souls_formats_test_crypto` so the requested build command has stable phase-level targets.
- KRAK compression defaults to Kraken level 6 (Elden Ring/Sekiro common path); AC6 level 9 can be added later as an explicit option if the public API grows KRAK parameters.

## [2026-05-10] Magic-driven DCX unwrap API

- Added `sf_dcx_unwrap` as a public DCX API rather than baking format chains into regulation tests or crypto code. This keeps decompression layer decisions in the compression module and driven by magic numbers.
- Kept `sf_dcx_sniff` semantics unchanged; unwrap is a consumer of sniff/decompress and does not introduce new enum values.

## [2026-05-10] Unified DCX compression params

- Replaced the DFLT-only public params API with unified `sf_dcx_params_t` plus `sf_dcx_read_params`/`sf_dcx_compress_ex` so KRAK/ZSTD levels can round-trip from headers too.
- Kept `sf_dcx_compress(type, ...)` as the simple default API. Defaults remain: DFLT 11000/44/9, KRAK level 6, ZSTD level 15.
- Did not add a public Oodle compressor selector: DCX `KRAK` magic self-identifies the Kraken compressor; only Oodle level is variable in the DCX header.
