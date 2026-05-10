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
| `public class BND4 : SoulsFile<BND4>, IBinder, IBND4` | BND4.cs:9 | Class | `sf_bnd4_t` eager API | ✓ aligned | Phase 3 target |
| `public List<BinderFile> Files { get; set; }` | BND4.cs:14 | Property | `sf_bnd4_file_count/get_file/add_file/remove_file` | ✓ aligned | |
| `public string Version { get; set; }` | BND4.cs:19 | Property | `sf_bnd4_get/set_version` | ✓ aligned | |
| `public Binder.Format Format { get; set; }` | BND4.cs:24 | Property | `sf_bnd4_get/set_format` | ✓ aligned | |
| `public bool BigEndian { get; set; }` | BND4.cs:39 | Property | `sf_bnd4_get/set_big_endian` | ✓ aligned | |
| `public bool BitBigEndian { get; set; }` | BND4.cs:44 | Property | `sf_bnd4_get/set_bit_big_endian` | ✓ aligned | Inverted on disk per upstream |
| `public bool Unicode { get; set; }` | BND4.cs:49 | Property | `sf_bnd4_get/set_unicode` | ✓ aligned | |
| `public byte Extended { get; set; }` | BND4.cs:54 | Property | `sf_bnd4_get/set_extended` | ✓ aligned | 0/1/4/0x80 accepted |
| `internal static bool IsFormat(BinaryReaderEx br)` | BND4.cs:71 | Method | BND4 magic assertion in read paths | ✓ aligned | |
| `protected override void Read(BinaryReaderEx br)` | BND4.cs:83 | Method | `sf_bnd4_read_from_memory/path` | ✓ aligned | DCX auto-unwrap |
| `internal static List<BinderFileHeader> ReadHeader(IBND4 bnd, BinaryReaderEx br)` | BND4.cs:91 | Method | internal `bnd4_read_header` | ✓ aligned | |
| `protected override void Write(BinaryWriterEx bw)` | BND4.cs:145 | Method | `sf_bnd4_write_to_memory/path` | ✓ aligned | |
| `internal static void WriteHeader(IBND4 bnd, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BND4.cs:156 | Method | internal `bnd4_write_to_writer` | ✓ aligned | |
| `public class BND4Reader : BinderReader, IBND4` | BND4Reader.cs:9 | Class | `sf_bnd4_reader_t` streaming API | ✓ aligned | Phase 3 target |
| `public static BND4Reader Read(string path)` | BND4Reader.cs:73 | Method | `sf_bnd4_reader_open` | ✓ aligned | |
| `public static BND4Reader Read(byte[] bytes)` | BND4Reader.cs:79 | Method | `sf_bnd4_read_from_memory` | ✓ aligned | Eager C API covers byte buffer |
| `public static BND4Reader Read(Stream stream)` | BND4Reader.cs:85 | Method | `sf_bnd4_reader_open` / path-backed stream | ✓ aligned | C public API exposes path-backed reader |
