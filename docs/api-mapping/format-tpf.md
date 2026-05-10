# TPF Mapping

| Upstream file | Upstream loc |
| :--- | :--- |
| `TPF.cs` | `SoulsFormats/Formats/TPF/TPF.cs` |
| `Headerizer.cs` | `SoulsFormats/Formats/TPF/Headerizer.cs` |
| `DDS.cs` | `SoulsFormats/Formats/TPF/DDS.cs` |
| `SecretHeaderizer.cs` | `SoulsFormats/Formats/TPF/SecretHeaderizer.cs` |

## API Mapping

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public partial class TPF` | `TPF.cs:11` | class | `sf_tpf_t` | ✓ aligned | Opaque handle, deep-copy semantics. |
| `public List<Texture> Textures` | `TPF.cs:16` | property | `sf_tpf_texture_count` / `sf_tpf_get_texture` / `sf_tpf_add_texture` / `sf_tpf_remove_texture` | ✓ aligned | Index-based accessors mirror upstream `List<>` operations. |
| `public TPFPlatform Platform` | `TPF.cs:21` | property | `sf_tpf_get_platform` / `sf_tpf_set_platform` | ✓ aligned | |
| `public byte Encoding` | `TPF.cs:26` | property | `sf_tpf_get_encoding` / `sf_tpf_set_encoding` | ✓ aligned | |
| `public byte Flag2` | `TPF.cs:31` | property | `sf_tpf_get_flag2` / `sf_tpf_set_flag2` | ✓ aligned | |
| `public TPF()` | `TPF.cs:36` | ctor | `sf_tpf_create` | ✓ aligned | Defaults Platform=PC, Encoding=1, Flag2=3. |
| `protected override bool Is(BinaryReaderEx)` | `TPF.cs:47` | method | _internal_ | ✓ aligned | Magic "TPF\0" verified inside `sf_tpf_read_*`. |
| `protected override void Read(BinaryReaderEx)` | `TPF.cs:59` | method | `sf_tpf_read_from_memory` / `sf_tpf_read_from_path` | ✓ aligned | PC fully decoded; PS3/Xbox360/PS4/Xbox1/PS5 metadata blocks are skipped past so the cursor lands correctly, but their texture-header fields are not preserved on round-trip. Switch heuristic from `TPF.cs:74` not implemented. |
| `protected override void Write(BinaryWriterEx)` | `TPF.cs:104` | method | `sf_tpf_write_to_memory` / `sf_tpf_write_to_path` | ✓ aligned | PC byte-equal round-trip verified. PS3/PS4 padding rules implemented; their per-texture metadata is best-effort. |
| `public class Texture` | `TPF.cs:164` | class | `sf_tpf_texture_t` | ✓ aligned | Heap-owned, deep-copy via `sf_tpf_add_texture`. |
| `public string Name` | `TPF.cs:169` | property | `sf_tpf_texture_get_name` / `sf_tpf_texture_set_name` | ✓ aligned | UTF-8 on the boundary; encoded via `Encoding` byte on disk. |
| `public byte Format` | `TPF.cs:179` | property | `sf_tpf_texture_get_format` / `sf_tpf_texture_set_format` | ✓ aligned | |
| `public TexType Type` | `TPF.cs:184` | property | `sf_tpf_texture_get_cubemap` / `sf_tpf_texture_set_cubemap` | ~ partial | Only the Cubemap (=1) bit is exposed. Volume (=2) and TextureArray (=3) are not surfaced because the test scope did not exercise them; reading preserves them in the on-disk byte. |
| `public byte Mipmaps` | `TPF.cs:189` | property | `sf_tpf_texture_get_mipmap_count` / `sf_tpf_texture_set_mipmap_count` | ✓ aligned | |
| `public byte Flags1` | `TPF.cs:194` | property | `sf_tpf_texture_get_flags1` / `sf_tpf_texture_set_flags1` | ✓ aligned | DCP_EDGE compression at 2/3 wired through. |
| `public byte[] Bytes` | `TPF.cs:199` | property | `sf_tpf_texture_get_bytes` / `sf_tpf_texture_set_bytes` | ✓ aligned | DCP_EDGE auto-decompresses on read; re-wraps on write. |
| `public TexHeader Header` | `TPF.cs:204` | property | _skipped_ | _skipped_ | Console-only metadata; v1 PC scope omits this. |
| `public FloatStruct FloatStruct` | `TPF.cs:209` | property | _skipped_ | _skipped_ | Optional metadata; not emitted by writer (always 0). |
| `public Texture(TPFPlatform)` | `TPF.cs:214` | ctor | `sf_tpf_texture_create` | ✓ aligned | |
| `public Texture(string, byte, byte, byte[], TPFPlatform)` | `TPF.cs:225` | ctor | _internal Headerizer (PC only)_ | ~ partial | PC arm of the Headerizer is implemented (`sfi_tpf_headerize`); console arms return `SF_ERR_UNSUPPORTED_VERSION`. |
| `internal Texture(BinaryReaderEx, ...)` | `TPF.cs:292` | ctor | _internal_ | ✓ aligned | Mirrors per-platform header decoding for PC; cursor advance for PS3/Xbox360/PS4/Xbox1/PS5. |
| `internal void WriteHeader(BinaryWriterEx, ...)` | `TPF.cs:368` | method | _internal_ | ✓ aligned | PC variant fully wired; cubemap/mipmap derived from embedded DDS. |
| `internal void WriteName(BinaryWriterEx, ...)` | `TPF.cs:423` | method | _internal_ | ✓ aligned | UTF-16 / Shift-JIS per `Encoding`. |
| `internal int WriteData(BinaryWriterEx, ...)` | `TPF.cs:433` | method | _internal_ | ✓ aligned | DCP_EDGE re-wrap on write for flags1==2/3. |
| `public byte[] Headerize()` | `TPF.cs:451` | method | `sfi_tpf_headerize` | ~ partial | Internal-only entry; PC pass-through implemented, console arms return `SF_ERR_UNSUPPORTED_VERSION` (extension; see [`extensions.md`](extensions.md)). |
| `public byte[] HeaderizeExt(out string)` | `TPF.cs:459` | method | _skipped_ | _skipped_ | Extension-string output not surfaced; PC consumers know the extension. |
| `public enum TPFPlatform` | `TPF.cs:476` | enum | `sf_tpf_platform_t` | ✓ aligned | All 6 platforms enumerated; `Switch=67` mapped to `SF_TPF_PLATFORM_UNKNOWN` (Switch heuristic not implemented). |
| `public enum TexType` | `TPF.cs:521` | enum | _flattened to `cubemap` bool_ | ~ partial | Only Cubemap exposed; Volume / TextureArray not surfaced in the public API. |
| `public class TexHeader` | `TPF.cs:547` | class | _skipped_ | _skipped_ | Console-only metadata. |
| `public class FloatStruct` | `TPF.cs:592` | class | _skipped_ | _skipped_ | Optional metadata. |
| `public static class Headerizer` | `Headerizer.cs:18` | class | `sfi_tpf_headerize` | ~ partial | PC pass-through arm only. Xbox360/Xbox1/PS3/PS4/PS5 swizzling and DDS reconstruction deferred. |
| `public class DDS` | `DDS.cs:9` | class | `_skipped_` | _skipped_ | DDS transports opaquely; `sfi_dds_parse_header` (internal) extracts only the metadata needed by TPF (cubemap, mipmap_count, depth). |
| `public static class SecretHeaderizer` | `SecretHeaderizer.cs:*` | class | _skipped_ | _skipped_ | Out of scope. |
