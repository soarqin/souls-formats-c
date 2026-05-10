# BND4 API Mapping

**Phase target**: 3
**Dependencies**: DCX, Binder Common

## Contributing Files
| File | Path |
|------|------|
| BND4.cs | SoulsFormats/Formats/Binder/BND4/BND4.cs |
| BND4Reader.cs | SoulsFormats/Formats/Binder/BND4/BND4Reader.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BND4 : SoulsFile<BND4>, IBinder, IBND4` | BND4.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public List<BinderFile> Files { get; set; }` | BND4.cs:14 | Property | 未实现 | 未实现 | |
| `public string Version { get; set; }` | BND4.cs:19 | Property | 未实现 | 未实现 | |
| `public Binder.Format Format { get; set; }` | BND4.cs:24 | Property | 未实现 | 未实现 | |
| `public bool BigEndian { get; set; }` | BND4.cs:39 | Property | 未实现 | 未实现 | |
| `public bool BitBigEndian { get; set; }` | BND4.cs:44 | Property | 未实现 | 未实现 | |
| `public bool Unicode { get; set; }` | BND4.cs:49 | Property | 未实现 | 未实现 | |
| `public byte Extended { get; set; }` | BND4.cs:54 | Property | 未实现 | 未实现 | |
| `internal static bool IsFormat(BinaryReaderEx br)` | BND4.cs:71 | Method | 未实现 | 未实现 | |
| `protected override void Read(BinaryReaderEx br)` | BND4.cs:83 | Method | 未实现 | 未实现 | |
| `internal static List<BinderFileHeader> ReadHeader(IBND4 bnd, BinaryReaderEx br)` | BND4.cs:91 | Method | 未实现 | 未实现 | |
| `protected override void Write(BinaryWriterEx bw)` | BND4.cs:145 | Method | 未实现 | 未实现 | |
| `internal static void WriteHeader(IBND4 bnd, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BND4.cs:156 | Method | 未实现 | 未实现 | |
| `public class BND4Reader : BinderReader, IBND4` | BND4Reader.cs:9 | Class | 未实现 | 未实现 | Phase 3 target |
| `public static BND4Reader Read(string path)` | BND4Reader.cs:73 | Method | 未实现 | 未实现 | |
| `public static BND4Reader Read(byte[] bytes)` | BND4Reader.cs:79 | Method | 未实现 | 未实现 | |
| `public static BND4Reader Read(Stream stream)` | BND4Reader.cs:85 | Method | 未实现 | 未实现 | |
