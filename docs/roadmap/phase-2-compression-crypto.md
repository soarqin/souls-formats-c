# Phase 2 — Compression + Crypto

> **Status**: ✅ done (2026-05-10) · **Depends on**: Phase 1

## Completion Retrospective

This phase implemented the DCX compression wrapper and the cryptographic helpers required for archive decryption. We successfully integrated zlib-ng, libzstd, and a dynamic loader for the Oodle Kraken DLL.

### Deliverables
* **Compression**: `sf_dcx.h`, `dcx.c`, `deflate_zlibng.c`, `deflate_chunked.c`, `zstd_wrap.c`.
* **Oodle**: `sf_oodle.h`, `oodle_loader.c` (v6, v8, v9 support).
* **Crypto**: `sf_regulation.h`, `sf_sl2.h`, `aes_cng.c`, `md5_cng.c`, `regulation.c`, `sl2.c`.
* **Tests**: 10 test binaries covering all compression types and crypto KATs.

## Alignment Status

The initial implementation was refined during Wave 3 to ensure strict alignment with upstream's object-oriented structures and enum definitions.

* **Mapping Docs**:
    * [DCX](../api-mapping/format-dcx.md)
    * [Oodle](../api-mapping/util-compression-oodle.md)
    * [RegulationDecryptor](../api-mapping/util-cryptography-regulation-decryptor.md)
    * [SL2Decryptor](../api-mapping/util-cryptography-sl2-decryptor.md)
    * [ZlibHelper](../api-mapping/util-compression-zlib-helper.md)
    * [ZstdHelper](../api-mapping/util-compression-zstd-helper.md)

* **Drift Resolution**: See [drift-checklist.md](../api-mapping/drift-checklist.md) for items closed by Tasks 15-20.

## Follow-up fixes pulled in by Wave 3

The following improvements were made to match upstream semantics:
* **DCX**: Replaced flat params with a tagged union (`sf_dcx_compression_info_t`) and added factory helpers.
* **Oodle**: Exposed `sf_oodle_lz_compressor_t` and other enums to the public API.
* **Regulation**: Added game-specific convenience wrappers for DS3, ER, and AC6.
* **SL2**: Promoted SL2 decryption to a public API with explicit key getters.

## Lessons Learned

* **Oodle ABI stability**: Different versions of `oo2core` have subtle parameter shifts; using version-specific thunks is necessary.
* **CNG IV handling**: Windows CNG requires the IV buffer to be writable, so we must always use a scratch copy.
* **DCX Chunked Deflate**: The `DCP_EDGE` and `DCX_EDGE` formats are unique to FromSoftware and required a careful manual port of the upstream chunking logic.
* **Deterministic compression**: Oodle compression is non-deterministic at high levels; tests must verify decompression equality rather than ciphertext equality.
