# BND3 API Mapping

**Phase target**: 3
**Dependencies**: DCX, Binder Common

## Contributing Files
| File | Path |
|------|------|
| BND3.cs | SoulsFormats/Formats/Binder/BND3/BND3.cs |
| BND3Reader.cs | SoulsFormats/Formats/Binder/BND3/BND3Reader.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BND3 : SoulsFile<BND3>, IBinder, IBND3` | BND3.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public List<BinderFile> Files { get; set; }` | BND3.cs:14 | Property | 未实现 | 未实现 | |
| `public string Version { get; set; }` | BND3.cs:19 | Property | 未实现 | 未实现 | |
| `public Binder.Format Format { get; set; }` | BND3.cs:24 | Property | 未实现 | 未实现 | |
| `public bool BigEndian { get; set; }` | BND3.cs:29 | Property | 未实现 | 未实现 | |
| `public bool BitBigEndian { get; set; }` | BND3.cs:34 | Property | 未实现 | 未实现 | |
| `public int Unk18 { get; set; }` | BND3.cs:39 | Property | 未实现 | 未实现 | |
| `public bool WriteFileHeadersEnd { get; set; }` | BND3.cs:45 | Property | 未实现 | 未实现 | |
| `internal static bool IsFormat(BinaryReaderEx br)` | BND3.cs:60 | Method | 未实现 | 未实现 | |
| `protected override void Read(BinaryReaderEx br)` | BND3.cs:72 | Method | 未实现 | 未实现 | |
| `internal static List<BinderFileHeader> ReadHeader(IBND3 bnd, BinaryReaderEx br)` | BND3.cs:80 | Method | 未实现 | 未实现 | |
| `protected override void Write(BinaryWriterEx bw)` | BND3.cs:109 | Method | 未实现 | 未实现 | |
| `internal static void WriteHeader(IBND3 bnd, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BND3.cs:120 | Method | 未实现 | 未实现 | |
| `public class BND3Reader : BinderReader, IBND3` | BND3Reader.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public static BND3Reader Read(string path)` | BND3Reader.cs:64 | Method | 未实现 | 未实现 | |
| `public static BND3Reader Read(byte[] bytes)` | BND3Reader.cs:70 | Method | 未实现 | 未实现 | |
| `public static BND3Reader Read(Stream stream)` | BND3Reader.cs:76 | Method | 未实现 | 未实现 | |
