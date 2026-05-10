# Binder Common API Mapping

**Phase target**: 3
**Dependencies**: DCX, HashHelper

## Contributing Files
| File | Path |
|------|------|
| Binder.cs | SoulsFormats/Formats/Binder/Binder.cs |
| BinderFile.cs | SoulsFormats/Formats/Binder/BinderFile.cs |
| BinderFileHeader.cs | SoulsFormats/Formats/Binder/BinderFileHeader.cs |
| BinderHashTable.cs | SoulsFormats/Formats/Binder/BinderHashTable.cs |
| BinderReader.cs | SoulsFormats/Formats/Binder/BinderReader.cs |
| IBinder.cs | SoulsFormats/Formats/Binder/IBinder.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public enum Format : byte` | Binder.cs:17 | Enum | 未实现 | 未实现 | Binder format flags |
| `public static Format ReadFormat(BinaryReaderEx br, bool bitBigEndian)` | Binder.cs:68 | Method | 未实现 | 未实现 | |
| `public static void WriteFormat(BinaryWriterEx bw, bool bitBigEndian, Format format)` | Binder.cs:78 | Method | 未实现 | 未实现 | |
| `public enum FileFlags : byte` | Binder.cs:137 | Enum | 未实现 | 未实现 | Per-file flags |
| `public static FileFlags ReadFileFlags(BinaryReaderEx br, bool bitBigEndian)` | Binder.cs:188 | Method | 未实现 | 未实现 | |
| `public static void WriteFileFlags(BinaryWriterEx bw, bool bitBigEndian, FileFlags flags)` | Binder.cs:198 | Method | 未实现 | 未实现 | |
| `public class BinderFile` | BinderFile.cs:8 | Class | 未实现 | 未实现 | Generic file container |
| `public class BinderFileHeader` | BinderFileHeader.cs:8 | Class | 未实现 | 未实现 | File metadata |
| `internal static class BinderHashTable` | BinderHashTable.cs:7 | Class | 未实现 | 未实现 | BND4/BXF4 hash table |
| `public abstract class BinderReader` | BinderReader.cs:9 | Class | 未实现 | 未实现 | On-demand reader base |
| `public interface IBinder` | IBinder.cs:8 | Interface | 未实现 | 未实现 | Binder interface |
| `public static DateTime BinderTimestampToDate(string timestamp)` | Binder.cs:215 | Method | 未实现 | 未实现 | |
| `public static string DateToBinderTimestamp(DateTime dateTime)` | Binder.cs:233 | Method | 未实现 | 未实现 | |
