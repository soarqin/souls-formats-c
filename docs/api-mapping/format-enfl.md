# ENFL Mapping

| Upstream file | Upstream loc |
| :--- | :--- |
| `ENFL.cs` | `SoulsFormats/Formats/ENFL.cs` |

## API Mapping

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public class ENFL` | `ENFL.cs:9` | class | `sf_enfl_t` | ✓ aligned | Opaque handle |
| `public List<Struct1> Struct1s` | `ENFL.cs:14` | property | `sf_enfl_struct1_count` / `sf_enfl_get_struct1` / `sf_enfl_add_struct1` | ✓ aligned | C-idiomatic accessor split |
| `public List<Struct2> Struct2s` | `ENFL.cs:19` | property | `sf_enfl_struct2_count` / `sf_enfl_get_struct2` / `sf_enfl_add_struct2` | ✓ aligned | |
| `public List<string> Strings` | `ENFL.cs:24` | property | `sf_enfl_string_count` / `sf_enfl_get_string` / `sf_enfl_add_string` | ✓ aligned | UTF-8 boundary; UTF-16LE on disk |
| `protected override bool Is(BinaryReaderEx br)` | `ENFL.cs:29` | method | _skipped_ | _skipped_ | Sniff omitted in v0.x; magic check inlined into reader |
| `protected override void Read(BinaryReaderEx br)` | `ENFL.cs:41` | method | `sf_enfl_read_from_path` / `sf_enfl_read_from_memory` | ✓ aligned | Internal zlib (NOT DCX) |
| `protected override void Write(BinaryWriterEx bw)` | `ENFL.cs:77` | method | `sf_enfl_write_to_path` / `sf_enfl_write_to_memory` | ✓ aligned | Inner payload built into a sub-buffer, then zlib-compressed |
| `public class Struct1` | `ENFL.cs:112` | class | `sf_enfl_struct1_t` | ✓ aligned | POD struct: int16 step, int16 index |
| `public short Step` | `ENFL.cs:117` | field | `sf_enfl_struct1_t::step` | ✓ aligned | |
| `public short Index` | `ENFL.cs:122` | field | `sf_enfl_struct1_t::index` | ✓ aligned | |
| `public class Struct2` | `ENFL.cs:150` | class | `sf_enfl_struct2_t` | ✓ aligned | POD struct: int64 unk1 |
| `public long Unk1` | `ENFL.cs:155` | field | `sf_enfl_struct2_t::unk1` | ✓ aligned | |
