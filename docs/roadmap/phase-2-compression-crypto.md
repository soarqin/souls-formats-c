# Phase 2 — Compression + Crypto

> **Status**: ✅ done · 13/13 PASS across 13 binaries (verified 2026-05-10) · **Depends on**: Phase 1

## Goal

Implement the entire DCX compression-wrapper machinery (zlib, chunked
deflate, Zstandard, Oodle Kraken) plus the AES / MD5 helpers built on
Windows CNG. After this phase, the library can decompress any DCX-wrapped
file and decrypt `regulation.bin`.

This is the second-most critical phase: every downstream format module
sits behind some flavour of DCX, and the BHD5 archives in Phase 3 need
AES-128-ECB.

---

## Deliverables

### Compression
- [ ] `include/souls_formats/sf_dcx.h` — public type `sf_dcx_type_t`
      (`None / Zlib / DCP_EDGE / DCX_EDGE / DCP_DFLT / DCX_DFLT / DCX_KRAK / DCX_ZSTD`).
- [ ] `sf_dcx_decompress(in, in_size, out_buf, out_size, *type)`.
- [ ] `sf_dcx_compress(in, in_size, type, out_buf, out_size)`.
- [ ] `sf_dcx_sniff(buf, size, *type)` — header-only inspection.
- [ ] zlib-ng wrapper: raw deflate + zlib-wrapped, both directions.
- [ ] `DCP_EDGE` / `DCX_EDGE` chunked deflate (see upstream `DCX.cs` lines
      244–374; manually port the chunk table layout).
- [ ] libzstd wrapper: single-frame compress + decompress.
- [ ] **Oodle DLL loader**:
      - Probe order: `oo2core_9_win64.dll` → `oo2core_8_win64.dll` → `oo2core_6_win64.dll`.
      - Search path: process dir → `sf_oodle_set_search_path()` value → `PATH`.
      - On miss, return `SF_ERR_OODLE_NOT_FOUND`. **Never** auto-fallback to
        an in-tree decoder; the GPL story is too risky and `ooz` only handles
        a subset.
      - Functions imported: `OodleLZ_Decompress`, `OodleLZ_Compress`,
        `OodleLZ_GetCompressedBufferSizeNeeded`,
        `OodleLZ_CompressOptions_GetDefault`, `OodleLZ_GetDecodeBufferSize`.
- [ ] `sf_oodle_set_search_path(const wchar_t *)` public API.

### Crypto
- [ ] `src/crypto/aes_cng.c` — AES-128/256-ECB/CBC over BCrypt.
- [ ] `src/crypto/md5_cng.c` — MD5 over BCrypt.
- [ ] `src/crypto/regulation.c` — DS3 / ER / AC6 / Nightreign regulation
      AES-256-CBC (key constants in `src/crypto/regulation_keys.c`).
- [ ] `src/crypto/sl2.c` — SL2 save AES-128-CBC + MD5 integrity.

---

## File structure

```
include/souls_formats/
├── sf_dcx.h              ← public: types + 3 functions
└── sf_oodle.h            ← public: search-path control + version probe
src/
├── compression/
│   ├── dcx.c             ← top-level dispatcher: sniff → route → (de)compress
│   ├── deflate_zlibng.c  ← zlib-ng wrap, both modes
│   ├── deflate_chunked.c ← DCP_EDGE / DCX_EDGE
│   ├── zstd_wrap.c       ← libzstd wrap
│   └── oodle/
│       ├── oodle_loader.c ← LoadLibraryW, GetProcAddress, version dispatch
│       ├── oodle_v6.c     ← entry-point thunks for oo2core_6_win64.dll
│       ├── oodle_v8.c     ← thunks for oo2core_8_win64.dll
│       └── oodle_v9.c     ← thunks for oo2core_9_win64.dll
└── crypto/
    ├── aes_cng.c
    ├── md5_cng.c
    ├── regulation.c
    ├── regulation_keys.c  ← const arrays of AES-256 keys, per game
    └── sl2.c
tests/
├── compression/
│   ├── test_dcx_dflt.c
│   ├── test_dcx_edge.c
│   ├── test_dcx_zstd.c
│   ├── test_dcx_krak.c
│   └── test_dcx_sniff.c
└── crypto/
    ├── test_aes_kat.c
    ├── test_md5_kat.c
    └── test_regulation_decrypt.c
```

---

## Upstream references (READ FIRST)

| File | Why |
|---|---|
| `SoulsFormats/Formats/DCX.cs` (entire file) | Top-level dispatcher; lines 244–374 hold the chunked deflate logic for `DCP_EDGE` / `DCX_EDGE` |
| `SoulsFormats/Utilities/Compression/ZlibHelper.cs` | Zlib + deflate wrappers |
| `SoulsFormats/Utilities/Compression/ZstdHelper.cs` | Zstandard wrap |
| `SoulsFormats/Utilities/Compression/Oodle/Oodle.cs` | Loader / version detection |
| `SoulsFormats/Utilities/Compression/Oodle/Oodle{26,28,29}.cs` | P/Invoke per major Oodle version |
| `SoulsFormats/Utilities/NativeLibrary.cs` | DLL search-path helper |
| `SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs` | regulation.bin AES-256-CBC |
| `SoulsFormats/Utilities/Cryptography/SL2Decryptor.cs` | SL2 AES-128-CBC + MD5 |
| `SoulsFormats/Formats/BHD5.cs` (look only at the AES section) | Exposes the salt/key scheme for Phase 3; useful context |

---

## Public API sketch

```c
/* sf_dcx.h */
typedef enum sf_dcx_type {
    SF_DCX_TYPE_NONE = 0,
    SF_DCX_TYPE_ZLIB,
    SF_DCX_TYPE_DCP_EDGE,
    SF_DCX_TYPE_DCX_EDGE,
    SF_DCX_TYPE_DCP_DFLT,
    SF_DCX_TYPE_DCX_DFLT,
    SF_DCX_TYPE_DCX_KRAK,
    SF_DCX_TYPE_DCX_ZSTD,
    SF_DCX_TYPE_UNKNOWN,
} sf_dcx_type_t;

SF_API sf_result_t sf_dcx_sniff(const void *buf, size_t size,
                                sf_dcx_type_t *out_type);

SF_API sf_result_t sf_dcx_decompress(const void *in, size_t in_size,
                                     void **out, size_t *out_size,
                                     sf_dcx_type_t *out_type,
                                     const sf_allocator_t *a);

SF_API sf_result_t sf_dcx_compress(const void *in, size_t in_size,
                                   sf_dcx_type_t type,
                                   void **out, size_t *out_size,
                                   const sf_allocator_t *a);

/* sf_oodle.h */
SF_API sf_result_t sf_oodle_set_search_path(const wchar_t *dir);
SF_API sf_result_t sf_oodle_load(void);     /* explicit; usually lazy */
SF_API void        sf_oodle_unload(void);   /* for tests */
SF_API int         sf_oodle_version(void);  /* 6/8/9 or 0 */
```

Internal-only crypto interface (do not expose AES through the public API
for v1; format modules use it directly):

```c
/* src/crypto/aes_cng.h (internal) */
sf_result_t sfi_aes_decrypt_cbc(const void *key, size_t key_len,
                                const void *iv, const void *in, size_t n,
                                void *out);
sf_result_t sfi_aes_encrypt_cbc(...);
sf_result_t sfi_aes_ecb_block(const void *key, size_t key_len,
                              bool encrypt, const uint8_t in[16], uint8_t out[16]);
```

---

## Implementation notes

* **DCP_EDGE / DCX_EDGE chunked deflate** is hand-rolled in upstream and
  needs to be hand-rolled here too. Read those 130 lines carefully — the
  chunk table lives between magic + body and the offsets are 0x10-aligned.
* **Oodle DLL search**: keep the loader idempotent. `sf_oodle_load()` may
  be called many times; track a "tried but failed" state to avoid hammer
  retries. `sf_oodle_unload()` exists for tests only — they need to verify
  the `SF_ERR_OODLE_NOT_FOUND` path.
* **Oodle compress** for KRAK uses `OodleLZ_Compressor::Kraken` (= 8 in
  upstream's enum). Compression level 4 matches DS3/ER. AC6 uses a higher
  preset (`DCX_KRAK_MAX` in folklore = level 9); pin to the same level via
  `OodleLZ_CompressOptions`.
* **regulation.bin**: first 16 bytes are the IV, the rest is AES-256-CBC
  ciphertext over a BND4 archive. Each game has its own 32-byte key
  constant; Sekiro is the only target without this format. Reuse the
  community-known keys committed in `regulation_keys.c`.
* **CNG init/teardown**: each `BCryptOpenAlgorithmProvider` returns a
  handle; cache it lazily per algorithm and close on `DllMain` detach
  (or expose `sfi_crypto_global_shutdown()` callable from tests).
* **DCX ZSTD frame**: upstream wraps a single zstd frame inside the DCX
  envelope. Use `ZSTD_decompress` / `ZSTD_compress` directly; do NOT
  enable streaming for v1 — the DCX header always knows the uncompressed
  size up front.

---

## QA scenarios

Tools: `cmake / ninja / ctest / WSL interop` plus
`/home/soar/dev/oodle/oo2core_*_win64.dll` and `/mnt/c/Games/ELDEN RING/`.

```bash
cmake --build build-mingw \
      --target souls_formats_test_compression souls_formats_test_crypto
ctest --test-dir build-mingw -L 'compression|crypto' --output-on-failure
```

### Compression
* `test_dcx_dflt` — three buffer sizes (1 KB / 16 KB / 1 MB) × three entropies
  (random / zero / all-0xFF), compress → decompress, byte-equal.
* `test_dcx_edge` — synthetic 2-chunk × 8 KB fixture matching upstream
  `DCX.cs` 244–374; round-trip byte-equal.
* `test_dcx_zstd` — single zstd frame round-trip.
* `test_dcx_krak` — Oodle compress → decompress at three sizes, asserting
  byte-equality and `type == SF_DCX_TYPE_DCX_KRAK`. **No game file**: this
  uses Oodle's own compression, not real Elden Ring data. Real KRAK on
  game data is implicitly tested in Phase 3 via BHD5 extraction.
* `test_dcx_sniff` — every fixture above passes through `sf_dcx_sniff`
  and returns the constructed type.

### Crypto
* `test_aes_kat` — NIST CAVP `AES{128,256}-{ECB,CBC}.rsp`, ≥20 vectors,
  byte-equal.
* `test_md5_kat` — RFC 1321 §A.5, all 7 vectors.
* `test_regulation_decrypt` — read `/mnt/c/Games/ELDEN RING/Game/regulation.bin`
  (~2.0 MB), AES-256-CBC decrypt with embedded ER key, decrypted bytes
  start with `BND4` magic. Re-encrypt and assert byte-equal round-trip.

### Skip rules
* `~/dev/oodle/oo2core_6_win64.dll` missing → `test_dcx_krak` SKIP via
  `TEST_IGNORE_MESSAGE("oodle dll missing")`.
* `/mnt/c/Games/ELDEN RING/Game/regulation.bin` missing →
  `test_regulation_decrypt` SKIP.

---

## Risks

| Risk | Mitigation |
|---|---|
| Oodle DLL ABI varies between v6/v8/v9 (subtle parameter shifts) | One thunk file per version; loader picks the matching `IOodleCompressor` vtable |
| AES key for ER known in community wikis but never publicly attested | Embed verbatim from upstream `RegulationDecryptor.cs`; if upstream is wrong, all consumers are wrong |
| `BCryptDecrypt` requires the IV buffer to be writable | Always copy IV into a stack scratch before passing to CNG |
| `LoadLibraryW` with bare DLL name searches PATH; worst-case loads a malicious DLL | Always prepend explicit search dir via `SetDllDirectoryW` or `LoadLibraryExW(LOAD_LIBRARY_SEARCH_USER_DIRS)` |
| Oodle compress is non-deterministic at level 9 (parallel encoder) | Tests must compare *decompress(compress(x)) == x*, not *compress(x) == fixed bytes* |

---

## Exit criteria

- [ ] All deliverables checked off above.
- [ ] `ctest -L 'compression|crypto'` reports all green or properly skipped
      (with a printed reason) on the dev machine.
- [ ] `lsp_diagnostics` clean across every changed file.
- [ ] DLL exports include the new `sf_dcx_*` and `sf_oodle_*` symbols
      (verify via `objdump -p libsouls_formats.dll | grep sf_dcx`).
- [ ] `PLAN.md` Phase 2 checkboxes ticked with timestamp + actual test counts.

When all green, proceed to [Phase 3](phase-3-archive-containers.md).
