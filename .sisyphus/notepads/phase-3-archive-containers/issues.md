# Phase 3 Issues & Gotchas

## Known Gotchas (from Plan Research)

### BHD5
- RSA decrypt uses BCRYPT_PAD_NONE (raw modular exp), NOT BCRYPT_PAD_PKCS1 or BCRYPT_PAD_OAEP
- Data0.bhd first 4 bytes = `e1 0e 36 ab` when RSA-encrypted (ER)
- `is64Bit` auto-detection at offset 0x14 — three i32 reads; if test0==0 && test1==0 → 64-bit
- AES range: start_offset == end_offset is LEGAL (skip, don't error)
- AES range: must bounds-check end_offset <= bdt_file_length
- SHA blobs: 32 bytes stored verbatim; never compare or recompute
- BCrypt provider must be cached (not opened per call)

### BND4/BXF4
- `!BitBigEndian` inversion at offset 0x0A (NOT direct storage)
- Names1 PC-save corner case (BinderFileHeader.cs:149-153): extra ID read + 0 padding
- Hash table ONLY when `extended >= 4`
- `Unk04` and `Unk05` are asserted booleans

### TPF
- DX10 cubemap fix at byte 0x8C is PC platform ONLY
- Per-texture DCP_EDGE wrap when Flags1==2 or ==3
- PS3/PS4/Xbox/PS5 Headerize paths → SF_ERR_UNSUPPORTED_VERSION

### ENFL
- Uses internal zlib, NOT DCX
- Magic "ENFL" + constant `0x10415` immediately after

### Binder Common
- `bw.Pad(0x10)` BEFORE file data when `bytes_size > 0` (BinderFileHeader.cs:236)
- BND3: 32-bit sizes/offsets; BND4: 64-bit sizes/offsets (when LongOffsets)
- Hash table prime: `for p = ceil(n/7); p <= 100000; if IsPrime(p) break`

### RSA Key Sources
- Keys are PUBLICLY KNOWN in soulsmods community (UXM-Selective-Unpack etc.)
- Must cite source URL in file header comment
- Only PUBLIC keys needed (decrypt-with-public = RSA verify-style)

### sf_path_hash_64
- SAME 32-bit algorithm as sf_path_hash, just zero-extended to uint64_t
- NOT a separate 64-bit algorithm

## Phase Dependency Notes
- T0a (sf_reverse_bits_u8) must complete before T1 (sf_binder.h)
- T0d (RSA) must complete before T3 (AES keys in same bhd5_keys.c file)
- T1+T2 must complete before T6-T9 (BND3/BND4/BXF3/BXF4)
- T0b+T0c+T0d+T3 must complete before T10 (BHD5)
- T0e+T1 must complete before T11 (TPF)

## T3 BLOCKER — Premise of "AES-128-ECB keys per game" appears incorrect (2026-05-10)

**Investigation summary:**
After thorough research of upstream and community sources, I cannot find any
public, per-game AES-128-ECB keys for BHD5 range decryption. The premise of T3
appears to be incorrect.

**Findings:**

1. **Upstream `BHD5.cs` stores AES keys INLINE in the file format**, not as
   per-game hardcoded constants:
   - `SoulsFormats/Formats/BHD5.cs` line 643+: `AESKey` is a class read from
     the BHD5 stream itself (`AESKey(BinaryReaderEx br)`).
   - Each `FileHeader` has an optional inline `AESKey` (line 459: `public AESKey AESKey { get; set; }`).
   - The 16-byte key is part of the file format's binary layout (line 662).
   - Decryption uses that inline key directly (line 689: `AESKey.Decrypt(byte[])`).

2. **`Nordgaren/UXM-Selective-Unpack/UXM/ArchiveKeys.cs` cited in
   `bhd5_keys.c`** contains ONLY RSA PEM keys for the .bhd outer wrapper —
   NO AES keys. Verified directly against the file at master branch.

3. **Original `JKAnderson/UXM/ArchiveKeys.cs`** (the upstream of Nordgaren's
   fork) likewise contains only RSA keys.

4. **`SoulsFormats/Utilities/Cryptography/`** in upstream has only:
   - `SL2Decryptor.cs` — DS2/Scholar/DS3 SL2 save keys (not BHD5, not v1 games)
   - `RegulationDecryptor.cs` — ER/AC6/Nightreign regulation.bin keys (32-byte
     AES-256, not 16-byte AES-128, not BHD5)

5. **GitHub broad search** for hardcoded per-game BHD5 AES keys in any
   community tool returned no results. The format simply does not use

## T10 gotchas confirmed (2026-05-10)

- The RSA helper currently unwraps one 2048-bit block at a time; BHD5 encrypted-file support loops over 256-byte blocks and concatenates stripped plaintext. A game-signed multi-block fixture is still needed for non-skipped RSA-wrapper coverage.
- AES range offsets are upstream-relative to the extracted file buffer; the reader also accepts absolute BDT offsets when they intersect the loaded slice, but synthetic tests should use relative offsets (`0..16`) to mirror `AESKey.Decrypt(byte[])`.
   per-game hardcoded AES keys.

**Conclusion:**
The T3 task constraints conflict:
- "Add 4 AES-128-ECB keys (one per v1 game)" requires per-game constants.
- "Make up AES key bytes — keys must be from legitimate community sources"
  forbids fabrication.
- No legitimate community source for such per-game keys exists, because the
  format stores AES keys inline.

**Possible resolutions (need orchestrator decision):**

a) **Drop T3.** The phase-3 BHD5 reader (T10) reads inline AES keys from the
   parsed file structure; no per-game key lookup is needed. Update T10 plan
   to remove the `sfi_bhd5_get_aes_key` reference; range decryption uses
   the inline key from each `FileHeader`.

b) **Reframe T3 as a no-op stub** that documents the format reality and
   declares the accessor returning NULL for all games (with TODO). The test
   then only asserts NULL across the board. This wastes effort and risks
   later confusion.

c) **Provide the orchestrator with the actual source** if there is one I
   missed (specific repo path/URL), and I'll proceed with that source.

I am pausing T3 and awaiting orchestrator guidance to avoid violating the
"no made up keys" constraint or producing dead code.

## 2026-05-11 — F3 Real Manual QA findings

QA run (Sisyphus-Junior) verifying Phase 3 deliverables against the plan.

### What passed
- Build: clean (`ninja: no work to do`).
- Full ctest: **32/32 PASS**, 0 failures, 8.57s total (logged in `evidence/final-qa/all-tests.log`).
- Core regression (Phase 0/1): **7/7 PASS**.
- Phase 2 (compression+crypto): **10/10 PASS**.
- Phase 3 archive synthetic round-trips: **8/8 PASS** (`binder_common`, `bhd5`, `bnd3`, `bnd4`, `bxf3`, `bxf4`, `enfl`, `tpf`).
- Phase 3 e2e: **5/5 binaries PASS** (er_helper_smoke + 4 keystone e2e_er). All inner Unity sub-tests gracefully `IGNORE` because ER copy/Oodle DLL not present — expected behavior.
- DLL exports: **469** `sf_*` symbols (well above 200 floor).
- Mapping docs: **0** files contain `未实现` markers across the 8 Phase 3 format docs.
- CLI binary `examples/sf_bnd_extract.exe` exists (5.2 MB), prints documented usage when invoked w/ no args, returns exit code 1 (per spec).

### Gap (does NOT block APPROVE, but tracked)
- Phase 3 plan deliverable `tests/examples/test_sf_bnd_extract_smoke.c` (synthetic deterministic CLI round-trip) is **not implemented**; the test `souls_formats_test_sf_bnd_extract_smoke` is not registered in `tests/CMakeLists.txt`. `ctest -R 'sf_bnd_extract_smoke'` returns "No tests were found!!!". The CLI binary itself is functional (manual verification passes), but there is no automated invariant proving end-to-end synthetic-fixture round-trip via the CLI surface. Recommend adding before declaring Phase 3 fully closed.

### Evidence files (11)
`evidence/final-qa/{build,all-tests,core-tests,phase2-tests,archive-tests,e2e-tests,keystone-e2e,cli-smoke,dll-exports,mapping-check}.log` + `EVIDENCE-LIST.txt`.

## 2026-05-11 — F4 upstream alignment spot-check findings

- BND4 critical fields and `!BitBigEndian` inversion match upstream; Names1 extra ID/zero trailer is implemented in `binder_common.c`.
- BHD5 spot-check rejected: current C code only models v1 ER-style file headers and drops SHA range metadata on read/write, so it does not fully mirror upstream BHD5 per-game layouts or SHA blob round-tripping.
- TPF spot-check rejected against the F4 scope note: current C code parses/writes non-PC metadata rather than returning `SF_ERR_UNSUPPORTED_VERSION` for non-PC TPFs.
- Binder hash table implementation mirrors upstream `files.Count / 7` integer-division start; note that this conflicts with the older notepad/spot-check wording that says `ceil(n/7)`.
