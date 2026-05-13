# BHD5 API Mapping

**Phase target**: 3
**Dependencies**: AES, HashHelper

## Contributing Files
| File | Path |
|------|------|
| BHD5.cs | SoulsFormats/Formats/BHD5.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BHD5` | BHD5.cs:11 | Class | `sf_bhd5_t` | ✓ aligned | Opaque C handle |
| `public enum Game` | BHD5.cs:16 | Enum | `sf_bhd5_game_t` | ✓ aligned | v1 game subset; legacy deferred |
| `public Game Format { get; set; }` | BHD5.cs:52 | Property | `sf_bhd5_open(..., game, ...)` | ✓ aligned | Immutable after open |
| `public bool BigEndian { get; set; }` | BHD5.cs:57 | Property | `sf_bhd5_get_big_endian` | ✓ aligned | Parsed from endian marker |
| `public string Salt { get; set; }` | BHD5.cs:67 | Property | `sf_bhd5_get_salt` | ✓ aligned | UTF-8/ASCII owned by handle |
| `public List<Bucket> Buckets { get; set; }` | BHD5.cs:72 | Property | `sf_bhd5_bucket_count`, `sf_bhd5_total_file_count` | ✓ aligned | Internal bucket/file metadata |
| `public static BHD5 Read(string path, Game game)` | BHD5.cs:107 | Method | `sf_bhd5_open` | ✓ aligned | Adds RSA unwrap extension before parse; default per-game key |
| n/a | n/a | + extension | `sf_bhd5_open_with_key` | + extension | Override RSA key for non-default archives (Sekiro Data2..Data5, DS3 DLC) |
| `public void Write(string path)` | BHD5.cs:188 | Method | `sf_bhd5_write` | ~ partial | Always writes 64-bit BHD5 header; DS3/Sekiro 32-bit-header writing pending |
| `public class Bucket : List<FileHeader>` | BHD5.cs:382 | Class | internal `sf_bhd5_bucket_t` | ✓ aligned | Count + file-header span |
| `public class FileHeader` | BHD5.cs:429 | Class | internal `sf_bhd5_file_t` | ✓ aligned | DS3/Sekiro 32-bit-hash + ER+ 64-bit-hash dual layout |
| `public class SHAHash` | BHD5.cs:594 | Class | internal `sha_hash[32]` | ✓ aligned | Stored opaque; not verified |
| `public class AESKey` | BHD5.cs:643 | Class | internal inline AES key + ranges | ✓ aligned | Per-file inline key, no game constants |
| `public struct Range` | BHD5.cs:710 | Struct | internal `sf_bhd5_range_t` | ✓ aligned | Start/end offsets; empty ranges skipped |
