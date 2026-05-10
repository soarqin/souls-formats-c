# BXF4 API Mapping

**Phase target**: 3
**Dependencies**: DCX, Binder Common

## Contributing Files
| File | Path |
|------|------|
| BXF4.cs | SoulsFormats/Formats/Binder/BXF4/BXF4.cs |
| BXF4Reader.cs | SoulsFormats/Formats/Binder/BXF4/BXF4Reader.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BXF4 : IBinder, IBXF4` | BXF4.cs:12 | Class | 未实现 | 未实现 | Phase 3 target |
| `public List<BinderFile> Files { get; set; }` | BXF4.cs:17 | Property | 未实现 | 未实现 | |
| `public string Version { get; set; }` | BXF4.cs:22 | Property | 未实现 | 未实现 | |
| `public Binder.Format Format { get; set; }` | BXF4.cs:27 | Property | 未实现 | 未实现 | |
| `public bool BigEndian { get; set; }` | BXF4.cs:42 | Property | 未实现 | 未实现 | |
| `public bool BitBigEndian { get; set; }` | BXF4.cs:47 | Property | 未实现 | 未实现 | |
| `public bool Unicode { get; set; }` | BXF4.cs:52 | Property | 未实现 | 未实现 | |
| `public byte Extended { get; set; }` | BXF4.cs:57 | Property | 未实现 | 未实现 | |
| `public static BXF4 Read(string bhdPath, string bdtBytes)` | BXF4.cs:90 | Method | 未实现 | 未实现 | |
| `internal static void ReadBDFHeader(BinaryReaderEx br)` | BXF4.cs:239 | Method | 未实现 | 未实现 | |
| `internal static List<BinderFileHeader> ReadBHFHeader(IBXF4 bxf, BinaryReaderEx br)` | BXF4.cs:257 | Method | 未实现 | 未实现 | |
| `public void Write(out byte[] bhdBytes, out byte[] bdtBytes)` | BXF4.cs:315 | Method | 未实现 | 未实现 | |
| `internal static void WriteBDFHeader(IBXF4 bxf, BinaryWriterEx bw)` | BXF4.cs:386 | Method | 未实现 | 未实现 | |
| `internal static void WriteBHFHeader(IBXF4 bxf, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BXF4.cs:405 | Method | 未实现 | 未实现 | |
| `public class BXF4Reader : BinderReader, IBXF4` | BXF4Reader.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public static BXF4Reader Read(string bhdPath, string bdtPath)` | BXF4Reader.cs:182 | Method | 未实现 | 未实现 | |
| `public static bool IsRead(string bhdPath, string bdtPath, out BXF4Reader reader)` | BXF4Reader.cs:236 | Method | 未实现 | 未实现 | |
