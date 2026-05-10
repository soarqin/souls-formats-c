# Phase 1 — Runtime Infrastructure

> **Status**: ✅ done (2026-05-10) · **Depends on**: Phase 0

## Completion Retrospective

This phase delivered the core IO, encoding, and math primitives that form the foundation of the library. We implemented the `sf_binary_reader_t` and `sf_binary_writer_t` types, which mirror the upstream `BinaryReaderEx` and `BinaryWriterEx` classes.

### Deliverables
* **IO**: `sf_io.h`, `stream.c`, `binary_reader.c`, `binary_writer.c`.
* **Encoding**: `sf_encoding.h`, `encoding_win32.c` (Shift-JIS, UTF-16, UTF-8).
* **Math**: `sf_math.h`, `math.c` (Vectors, Quaternions, Matrices).
* **Hashing**: `sf_hash.h`, `filename_hash.c` (FromPathHash).
* **Path**: `sf_path.h`, `path.c` (Normalization and backup).
* **Utility**: `sf_util.c` (Decompression helpers).

## Alignment Status

We achieved high alignment with upstream IO and utility classes. Remaining drift items were addressed during the Wave 2 realignment task.

* **Mapping Docs**:
    * [BinaryReaderEx](../api-mapping/util-io-binary-reader-ex.md)
    * [BinaryWriterEx](../api-mapping/util-io-binary-writer-ex.md)
    * [PathHelper](../api-mapping/util-io-path-helper.md)
    * [Encoding](../api-mapping/util-text-sf-encoding.md)
    * [HashHelper](../api-mapping/util-cryptography-hash-helper.md)
    * [SFUtil](../api-mapping/util-sf-util.md)

* **Drift Resolution**: See [drift-checklist.md](../api-mapping/drift-checklist.md) for items closed by Tasks 8-14.

## Follow-up fixes pulled in by Wave 2

The following APIs were added or renamed to match upstream more closely:
* **BinaryReaderEx**: Plural reads (read_i8s, etc.), Get* coverage, and multi-option assert signatures.
* **BinaryWriterEx**: Plural writes, Reserve/Fill for all primitive types, and 3-mode finish.
* **PathHelper**: Added `sf_path_backup`, `sf_path_get_real_extension`, and `sf_path_get_real_file_name`.
* **HashHelper**: Added `sf_is_prime`.
* **SFUtil**: Added `sf_get_decompressed_reader`.

## Lessons Learned

* **Endianness management**: Tracking big-endian state per-reader/writer instance proved more robust than global state.
* **Win32 encoding**: Using `MultiByteToWideChar` for Shift-JIS (CP 932) ensures perfect compatibility with game data.
* **Path normalization**: Handling both forward and backward slashes consistently is vital for cross-platform development on WSL2.
* **Reserve/Fill pattern**: The name-based reservation system in `BinaryWriterEx` requires careful internal tracking to prevent leaks.
