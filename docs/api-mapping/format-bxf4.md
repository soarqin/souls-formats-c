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
| `public class BXF4 : IBinder, IBXF4` | BXF4.cs:12 | Class | `sf_bxf4_t` | ✓ aligned | Opaque handle |
| `public List<BinderFile> Files { get; set; }` | BXF4.cs:17 | Property | `sf_bxf4_file_count` / `sf_bxf4_get_file` / `sf_bxf4_add_file` / `sf_bxf4_remove_file` | ✓ aligned | C-idiomatic accessor split |
| `public string Version { get; set; }` | BXF4.cs:22 | Property | `sf_bxf4_get_version` / `sf_bxf4_set_version` | ✓ aligned | |
| `public Binder.Format Format { get; set; }` | BXF4.cs:27 | Property | `sf_bxf4_get_format` / `sf_bxf4_set_format` | ✓ aligned | |
| `public bool Unk04 { get; set; }` | BXF4.cs:32 | Property | `sf_bxf4_get_unk04` / `sf_bxf4_set_unk04` | ✓ aligned | |
| `public bool Unk05 { get; set; }` | BXF4.cs:37 | Property | `sf_bxf4_get_unk05` / `sf_bxf4_set_unk05` | ✓ aligned | |
| `public bool BigEndian { get; set; }` | BXF4.cs:42 | Property | `sf_bxf4_get_big_endian` / `sf_bxf4_set_big_endian` | ✓ aligned | |
| `public bool BitBigEndian { get; set; }` | BXF4.cs:47 | Property | `sf_bxf4_get_bit_big_endian` / `sf_bxf4_set_bit_big_endian` | ✓ aligned | |
| `public bool Unicode { get; set; }` | BXF4.cs:52 | Property | `sf_bxf4_get_unicode` / `sf_bxf4_set_unicode` | ✓ aligned | |
| `public byte Extended { get; set; }` | BXF4.cs:57 | Property | `sf_bxf4_get_extended` / `sf_bxf4_set_extended` | ✓ aligned | |
| `public BXF4()` | BXF4.cs:62 | Constructor | `sf_bxf4_create` | ✓ aligned | Defaults: Version=binder timestamp, Format=IDs\|Names1\|Names2\|Compression, Unicode=true, Extended=4 |
| `public static BXF4 Read(string bhdPath, string bdtBytes)` | BXF4.cs:90 | Method | `sf_bxf4_read_from_paths` | ✓ aligned | Wide-path; auto DCX unwrap |
| `public static BXF4 Read(byte[] bhdBytes, byte[] bdtBytes)` | BXF4.cs:141 | Method | `sf_bxf4_read_from_memory` | ✓ aligned | |
| `internal static void ReadBDFHeader(BinaryReaderEx br)` | BXF4.cs:239 | Method | `bxf4_read_bdf_header` (static) | ✓ aligned | File-scope helper |
| `internal static List<BinderFileHeader> ReadBHFHeader(IBXF4 bxf, BinaryReaderEx br)` | BXF4.cs:257 | Method | `bxf4_read_bhf_header` (static) | ✓ aligned | File-scope helper |
| `public void Write(out byte[] bhdBytes, out byte[] bdtBytes)` | BXF4.cs:315 | Method | `sf_bxf4_write_to_memory` | ✓ aligned | |
| `public void Write(string bhdPath, string bdtPath)` | BXF4.cs:359 | Method | `sf_bxf4_write_to_paths` | ✓ aligned | Wide-path |
| `internal static void WriteBDFHeader(IBXF4 bxf, BinaryWriterEx bw)` | BXF4.cs:386 | Method | `bxf4_write_bdf_header` (static) | ✓ aligned | File-scope helper |
| `internal static void WriteBHFHeader(IBXF4 bxf, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BXF4.cs:405 | Method | `bxf4_write_bhf_header` (static) | ✓ aligned | File-scope helper |
| `public static bool IsHeader(string path)` | BXF4.cs:460 | Method | _skipped_ | _skipped_ | Sniff helpers omitted in v0.x; magic check inlined into reader |
| `public static bool IsData(string path)` | BXF4.cs:519 | Method | _skipped_ | _skipped_ | Same as above |
| `public class BXF4Reader : BinderReader, IBXF4` | BXF4Reader.cs:9 | Class | `sf_bxf4_reader_t` | ✓ aligned | Opaque handle, on-demand |
| `public BXF4Reader(string bhdPath, string bdtPath)` | BXF4Reader.cs:44 | Constructor | `sf_bxf4_reader_open` | ✓ aligned | |
| `public static BXF4Reader Read(string bhdPath, string bdtPath)` | BXF4Reader.cs:182 | Method | `sf_bxf4_reader_open` | ✓ aligned | Single ctor in C |
| `public static bool IsRead(string bhdPath, string bdtPath, out BXF4Reader reader)` | BXF4Reader.cs:236 | Method | _skipped_ | _skipped_ | Sniff variant omitted; caller can attempt open and inspect SF_ERR_BAD_MAGIC |
