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
| `public class BXF3 : IBinder, IBXF3` | BXF3.cs:12 | Class | `sf_bxf3_t` | ✓ aligned | Opaque handle |
| `public List<BinderFile> Files { get; set; }` | BXF3.cs:17 | Property | `sf_bxf3_file_count` / `sf_bxf3_get_file` / `sf_bxf3_add_file` / `sf_bxf3_remove_file` | ✓ aligned | C-idiomatic accessor split |
| `public string Version { get; set; }` | BXF3.cs:22 | Property | `sf_bxf3_get_version` / `sf_bxf3_set_version` | ✓ aligned | |
| `public Binder.Format Format { get; set; }` | BXF3.cs:27 | Property | `sf_bxf3_get_format` / `sf_bxf3_set_format` | ✓ aligned | |
| `public bool BigEndian { get; set; }` | BXF3.cs:32 | Property | `sf_bxf3_get_big_endian` / `sf_bxf3_set_big_endian` | ✓ aligned | |
| `public bool BitBigEndian { get; set; }` | BXF3.cs:37 | Property | `sf_bxf3_get_bit_big_endian` / `sf_bxf3_set_bit_big_endian` | ✓ aligned | |
| `public BXF3()` | BXF3.cs:42 | Constructor | `sf_bxf3_create` | ✓ aligned | Defaults: Version=binder timestamp, Format=IDs\|Names1\|Names2\|Compression |
| `public static BXF3 Read(string bhdPath, string bdtPath)` | BXF3.cs:68 | Method | `sf_bxf3_read_from_paths` | ✓ aligned | Wide-path; auto DCX unwrap |
| `public static BXF3 Read(byte[] bhdBytes, byte[] bdtBytes)` | BXF3.cs:119 | Method | `sf_bxf3_read_from_memory` | ✓ aligned | |
| `internal static void ReadBDFHeader(BinaryReaderEx br)` | BXF3.cs:215 | Method | `bxf3_read_bdf_header` (static) | ✓ aligned | File-scope helper |
| `internal static List<BinderFileHeader> ReadBHFHeader(IBXF3 bxf, BinaryReaderEx br)` | BXF3.cs:222 | Method | `bxf3_read_bhf_header` (static) | ✓ aligned | File-scope helper |
| `public void Write(out byte[] bhdBytes, out byte[] bdtBytes)` | BXF3.cs:255 | Method | `sf_bxf3_write_to_memory` | ✓ aligned | |
| `public void Write(string bhdPath, string bdtPath)` | BXF3.cs:299 | Method | `sf_bxf3_write_to_paths` | ✓ aligned | Wide-path |
| `internal static void WriteBDFHeader(IBXF3 bxf, BinaryWriterEx bw)` | BXF3.cs:326 | Method | `bxf3_write_bdf_header` (static) | ✓ aligned | File-scope helper |
| `internal static void WriteBHFHeader(IBXF3 bxf, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BXF3.cs:333 | Method | `bxf3_write_bhf_header` (static) | ✓ aligned | File-scope helper |
| `public static bool IsHeader(string path)` | BXF3.cs:364 | Method | _skipped_ | _skipped_ | Sniff helpers omitted in v0.x; magic check inlined into reader |
| `public static bool IsData(string path)` | BXF3.cs:424 | Method | _skipped_ | _skipped_ | Same as above |
| `public class BXF3Reader : BinderReader, IBXF3` | BXF3Reader.cs:9 | Class | `sf_bxf3_reader_t` | ✓ aligned | Opaque handle, on-demand |
| `public BXF3Reader(string bhdPath, string bdtPath)` | BXF3Reader.cs:24 | Constructor | `sf_bxf3_reader_open` | ✓ aligned | |
| `public static BXF3Reader Read(string bhdPath, string bdtPath)` | BXF3Reader.cs:162 | Method | `sf_bxf3_reader_open` | ✓ aligned | Single ctor in C |
| `public static bool IsRead(string bhdPath, string bdtPath, out BXF3Reader reader)` | BXF3Reader.cs:216 | Method | _skipped_ | _skipped_ | Sniff variant omitted; caller can attempt open and inspect SF_ERR_BAD_MAGIC |
