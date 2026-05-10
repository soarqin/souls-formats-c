# ZstdHelper Mapping

ZstdHelper is used internally by DCX; no public `sf_zstd_*` API is exposed. All functionality is hidden behind `sf_dcx_*`.

| Upstream signature | Upstream loc (ZstdHelper.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public static byte[] ReadZstd(BinaryReaderEx br, int compressedSize)` | 14 | Method | `sfi_zstd_decompress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
| `public static byte[] WriteZstd(byte[] data, int compressionLevel)` | 29 | Method | `sfi_zstd_compress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
