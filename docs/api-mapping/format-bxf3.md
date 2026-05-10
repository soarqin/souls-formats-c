# BXF3 API Mapping

**Phase target**: 3
**Dependencies**: DCX, Binder Common

## Contributing Files
| File | Path |
|------|------|
| BXF3.cs | SoulsFormats/Formats/Binder/BXF3/BXF3.cs |
| BXF3Reader.cs | SoulsFormats/Formats/Binder/BXF3/BXF3Reader.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BXF3 : IBinder, IBXF3` | BXF3.cs:12 | Class | 未实现 | 未实现 | Phase 3 target |
| `public List<BinderFile> Files { get; set; }` | BXF3.cs:17 | Property | 未实现 | 未实现 | |
| `public string Version { get; set; }` | BXF3.cs:22 | Property | 未实现 | 未实现 | |
| `public Binder.Format Format { get; set; }` | BXF3.cs:27 | Property | 未实现 | 未实现 | |
| `public bool BigEndian { get; set; }` | BXF3.cs:32 | Property | 未实现 | 未实现 | |
| `public bool BitBigEndian { get; set; }` | BXF3.cs:37 | Property | 未实现 | 未实现 | |
| `public static BXF3 Read(string bhdPath, string bdtBytes)` | BXF3.cs:68 | Method | 未实现 | 未实现 | |
| `internal static void ReadBDFHeader(BinaryReaderEx br)` | BXF3.cs:215 | Method | 未实现 | 未实现 | |
| `internal static List<BinderFileHeader> ReadBHFHeader(IBXF3 bxf, BinaryReaderEx br)` | BXF3.cs:222 | Method | 未实现 | 未实现 | |
| `public void Write(out byte[] bhdBytes, out byte[] bdtBytes)` | BXF3.cs:255 | Method | 未实现 | 未实现 | |
| `internal static void WriteBDFHeader(IBXF3 bxf, BinaryWriterEx bw)` | BXF3.cs:326 | Method | 未实现 | 未实现 | |
| `internal static void WriteBHFHeader(IBXF3 bxf, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BXF3.cs:333 | Method | 未实现 | 未实现 | |
| `public class BXF3Reader : BinderReader, IBXF3` | BXF3Reader.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public static BXF3Reader Read(string bhdPath, string bdtPath)` | BXF3Reader.cs:162 | Method | 未实现 | 未实现 | |
| `public static bool IsRead(string bhdPath, string bdtPath, out BXF3Reader reader)` | BXF3Reader.cs:216 | Method | 未实现 | 未实现 | |
