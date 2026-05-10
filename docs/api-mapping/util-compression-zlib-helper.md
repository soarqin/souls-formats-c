# ZlibHelper Mapping

ZlibHelper is used internally by DCX; no public `sf_zlib_*` API is exposed. All functionality is hidden behind `sf_dcx_*`.

| Upstream signature | Upstream loc (ZlibHelper.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public static int WriteZlib(BinaryWriterEx bw, byte formatByte, byte[] input)` | 14 | Method | `sfi_zlib_compress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
| `public static byte[] ReadZlib(BinaryReaderEx br, int compressedSize)` | 32 | Method | `sfi_zlib_decompress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
| `public static byte[] DecompressZlib(Stream stream, int compressedSize)` | 48 | Method | `sfi_zlib_decompress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
| `public static byte[] DecompressZlibBytes(byte[] compressedBytes)` | 81 | Method | `sfi_deflate_raw_decompress` | `+ extension` | internal-only by design (used by sf_dcx_*) |
| `public static uint Adler32(byte[] data)` | 95 | Method | `adler32_sf` | `+ extension` | internal-only by design (used by sf_dcx_*) |
