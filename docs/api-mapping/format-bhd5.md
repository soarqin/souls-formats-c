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
| `public class BHD5` | BHD5.cs:11 | Class | 未实现 | 未实现 | Phase 3 target |
| `public enum Game` | BHD5.cs:16 | Enum | 未实现 | 未实现 | BHD5 game variants |
| `public Game Format { get; set; }` | BHD5.cs:52 | Property | 未实现 | 未实现 | |
| `public bool BigEndian { get; set; }` | BHD5.cs:57 | Property | 未实现 | 未实现 | |
| `public string Salt { get; set; }` | BHD5.cs:67 | Property | 未实现 | 未实现 | |
| `public List<Bucket> Buckets { get; set; }` | BHD5.cs:72 | Property | 未实现 | 未实现 | |
| `public static BHD5 Read(string path, Game game)` | BHD5.cs:107 | Method | 未实现 | 未实现 | |
| `public void Write(string path)` | BHD5.cs:188 | Method | 未实现 | 未实现 | |
| `public class Bucket : List<FileHeader>` | BHD5.cs:382 | Class | 未实现 | 未实现 | |
| `public class FileHeader` | BHD5.cs:429 | Class | 未实现 | 未实现 | |
| `public class SHAHash` | BHD5.cs:594 | Class | 未实现 | 未实现 | |
| `public class AESKey` | BHD5.cs:643 | Class | 未实现 | 未实现 | |
| `public struct Range` | BHD5.cs:710 | Struct | 未实现 | 未实现 | |
