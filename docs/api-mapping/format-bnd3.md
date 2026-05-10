# BND3 API Mapping

**Phase target**: 3
**Dependencies**: DCX, Binder Common

## Contributing Files
| File | Path |
|------|------|
| BND3.cs | SoulsFormats/Formats/Binder/BND3/BND3.cs |
| BND3Reader.cs | SoulsFormats/Formats/Binder/BND3/BND3Reader.cs |
| IBND3.cs | SoulsFormats/Formats/Binder/BND3/IBND3.cs |

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public class BND3 : SoulsFile<BND3>, IBinder, IBND3` | BND3.cs:9 | Class | `sf_bnd3_t` | ✓ aligned | Opaque handle; lifetime via `sf_bnd3_create` / `sf_bnd3_destroy` |
| `public List<BinderFile> Files { get; set; }` | BND3.cs:14 | Property | `sf_bnd3_file_count` / `sf_bnd3_get_file` / `sf_bnd3_add_file` / `sf_bnd3_remove_file` | ✓ aligned | C-style accessor split (no public List<> in C) |
| `public string Version { get; set; }` | BND3.cs:19 | Property | `sf_bnd3_get_version` / `sf_bnd3_set_version` | ✓ aligned | UTF-8; default = current binder timestamp |
| `public Binder.Format Format { get; set; }` | BND3.cs:24 | Property | `sf_bnd3_get_format` / `sf_bnd3_set_format` | ✓ aligned | |
| `public bool BigEndian { get; set; }` | BND3.cs:29 | Property | `sf_bnd3_get_big_endian` / `sf_bnd3_set_big_endian` | ✓ aligned | |
| `public bool BitBigEndian { get; set; }` | BND3.cs:34 | Property | `sf_bnd3_get_bit_big_endian` / `sf_bnd3_set_bit_big_endian` | ✓ aligned | |
| `public int Unk18 { get; set; }` | BND3.cs:39 | Property | `sf_bnd3_get_unk18` / `sf_bnd3_set_unk18` | ✓ aligned | Asserts {0, 0x80000000} on read |
| `public bool WriteFileHeadersEnd { get; set; }` | BND3.cs:45 | Property | `sf_bnd3_get_write_file_headers_end` / `sf_bnd3_set_write_file_headers_end` | ✓ aligned | |
| `public BND3()` | BND3.cs:50 | Ctor | `sf_bnd3_create` | ✓ aligned | Default Format = IDs|Names1|Names2|Compression |
| `internal static bool IsFormat(BinaryReaderEx br)` | BND3.cs:60 | Method | `sf_bnd3_read_from_memory` returns `SF_ERR_BAD_MAGIC` | ✓ aligned | C-style adaptation: detection happens during read |
| `protected override void Read(BinaryReaderEx br)` | BND3.cs:72 | Method | `sf_bnd3_read_from_memory` / `sf_bnd3_read_from_path` | ✓ aligned | DCX wrapper auto-unwrapped |
| `internal static List<BinderFileHeader> ReadHeader(IBND3 bnd, BinaryReaderEx br)` | BND3.cs:80 | Method | internal `bnd3_read_header` (src/archive/bnd3.c) | ✓ aligned | Reused by both eager and reader paths |
| `protected override void Write(BinaryWriterEx bw)` | BND3.cs:109 | Method | `sf_bnd3_write_to_memory` / `sf_bnd3_write_to_path` | ✓ aligned | |
| `internal static void WriteHeader(IBND3 bnd, BinaryWriterEx bw, List<BinderFileHeader> fileHeaders)` | BND3.cs:120 | Method | internal `bnd3_write_to_writer` (src/archive/bnd3.c) | ✓ aligned | |
| `public class BND3Reader : BinderReader, IBND3` | BND3Reader.cs:9 | Class | `sf_bnd3_reader_t` | ✓ aligned | Opaque streaming handle |
| `public int Unk18 { get; set; }` | BND3Reader.cs:14 | Property | parsed during `sf_bnd3_reader_open`, stored on the handle | ✓ aligned | Accessor not exposed publicly yet (matches C-style minimal surface) |
| `public bool WriteFileHeadersEnd { get; set; }` | BND3Reader.cs:20 | Property | parsed during `sf_bnd3_reader_open`, stored on the handle | ✓ aligned | Same |
| `public DCX.CompressionInfo Compression { get; set; }` | BND3Reader.cs:25 | Property | parsed during `sf_bnd3_reader_open`, stored on the handle | ✓ aligned | Same |
| `private BND3Reader(BinaryReaderEx br)` | BND3Reader.cs:31 | Ctor | private inside `sf_bnd3_reader_open` | ✓ aligned | |
| `public BND3Reader(string path)` | BND3Reader.cs:39 | Ctor | `sf_bnd3_reader_open` | ✓ aligned | wchar_t path |
| `public BND3Reader(byte[] bytes)` | BND3Reader.cs:44 | Ctor | _skipped_ | _skipped_ | Reader from in-memory buffer not yet exposed (eager API covers buffer reads) |
| `public BND3Reader(Stream stream)` | BND3Reader.cs:49 | Ctor | _skipped_ | _skipped_ | C has no equivalent generic stream type |
| `public static BND3Reader Read(string path)` | BND3Reader.cs:64 | Method | `sf_bnd3_reader_open` | ✓ aligned | |
| `public static BND3Reader Read(byte[] bytes)` | BND3Reader.cs:70 | Method | _skipped_ | _skipped_ | See above |
| `public static BND3Reader Read(Stream stream)` | BND3Reader.cs:76 | Method | _skipped_ | _skipped_ | See above |
| `private static bool IsRead(BinaryReaderEx br, out BND3Reader reader)` | BND3Reader.cs:82 | Method | _skipped_ | _skipped_ | Caller reads + checks SF_ERR_BAD_MAGIC |
| `public static bool IsRead(string path, out BND3Reader reader)` | BND3Reader.cs:98 | Method | _skipped_ | _skipped_ | Same |
| `public static bool IsRead(byte[] bytes, out BND3Reader reader)` | BND3Reader.cs:108 | Method | _skipped_ | _skipped_ | Same |
| `public static bool IsRead(Stream stream, out BND3Reader reader)` | BND3Reader.cs:117 | Method | _skipped_ | _skipped_ | Same |
| `private void Read(BinaryReaderEx br)` | BND3Reader.cs:129 | Method | private inside `sf_bnd3_reader_open` | ✓ aligned | |
| (extension) — read entry by index | n/a | — | `sf_bnd3_reader_read_file_by_index` | + extension | C-idiomatic accessor |
| (extension) — read entry by ID | n/a | — | `sf_bnd3_reader_read_file_by_id` | + extension | C-idiomatic accessor |
| (extension) — header peek | n/a | — | `sf_bnd3_reader_get_file` / `sf_bnd3_reader_file_count` | + extension | C-idiomatic accessors |
