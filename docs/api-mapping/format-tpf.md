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
| `public partial class TPF` | `TPF.cs:11` | class | `sf_tpf_t` | 未实现 | Phase 3 |
| `public List<Texture> Textures` | `TPF.cs:16` | property | `sf_tpf_get_textures` | 未实现 | Phase 3 |
| `public TPFPlatform Platform` | `TPF.cs:21` | property | `sf_tpf_get_platform` | 未实现 | Phase 3 |
| `public byte Encoding` | `TPF.cs:26` | property | `sf_tpf_get_encoding` | 未实现 | Phase 3 |
| `public class Texture` | `TPF.cs:164` | class | `sf_tpf_texture_t` | 未实现 | Phase 3 |
| `public string Name` | `TPF.cs:169` | property | `sf_tpf_texture_get_name` | 未实现 | Phase 3 |
| `public byte[] Bytes` | `TPF.cs:199` | property | `sf_tpf_texture_get_bytes` | 未实现 | Phase 3 |
| `public enum TPFPlatform` | `TPF.cs:476` | enum | `sf_tpf_platform_t` | 未实现 | Phase 3 |
| `public static class Headerizer` | `Headerizer.cs:18` | class | `sf_tpf_headerizer_*` | 未实现 | Phase 3 |
| `public class DDS` | `DDS.cs:9` | class | `_skipped_` | _skipped_ | DDS transports opaquely; no pixel decoders. |
