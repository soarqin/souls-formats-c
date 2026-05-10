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
| `public enum Format : byte` | Binder.cs:17 | Enum | ✓ aligned | ✓ aligned | Binder format flags |
| `public static Format ReadFormat(BinaryReaderEx br, bool bitBigEndian)` | Binder.cs:68 | Method | ✓ aligned | ✓ aligned | |
| `public static void WriteFormat(BinaryWriterEx bw, bool bitBigEndian, Format format)` | Binder.cs:78 | Method | ✓ aligned | ✓ aligned | |
| `public enum FileFlags : byte` | Binder.cs:137 | Enum | ✓ aligned | ✓ aligned | Per-file flags |
| `public static FileFlags ReadFileFlags(BinaryReaderEx br, bool bitBigEndian)` | Binder.cs:188 | Method | ✓ aligned | ✓ aligned | |
| `public static void WriteFileFlags(BinaryWriterEx bw, bool bitBigEndian, FileFlags flags)` | Binder.cs:198 | Method | ✓ aligned | ✓ aligned | |
| `public class BinderFile` | BinderFile.cs:8 | Class | ✓ aligned | ✓ aligned | Generic file container |
| `public class BinderFileHeader` | BinderFileHeader.cs:8 | Class | ✓ aligned | ✓ aligned | File metadata |
| `internal static class BinderHashTable` | BinderHashTable.cs:7 | Class | ✓ aligned | ✓ aligned | BND4/BXF4 hash table |
| `public abstract class BinderReader` | BinderReader.cs:9 | Class | ✓ aligned | ✓ aligned | On-demand reader base |
| `public interface IBinder` | IBinder.cs:8 | Interface | ✓ aligned | ✓ aligned | Binder interface |
| `public static DateTime BinderTimestampToDate(string timestamp)` | Binder.cs:215 | Method | ✓ aligned | ✓ aligned | |
| `public static string DateToBinderTimestamp(DateTime dateTime)` | Binder.cs:233 | Method | ✓ aligned | ✓ aligned | |
