# PARAMDEF API Mapping

Upstream references:
- `SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs`
- `SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs`
- `SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs`
- `SoulsFormats/Formats/PARAM/ParamUtil.cs`

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public partial class PARAMDEF` | `PARAMDEF.cs:11` | class | `sf_paramdef_t` | ✓ aligned | Phase 4 |
| `public short DataVersion { get; set; }` | `PARAMDEF.cs:16` | property | `sf_paramdef_get_data_version` | ✓ aligned | Phase 4 |
| `public string ParamType { get; set; }` | `PARAMDEF.cs:21` | property | `sf_paramdef_get_param_type` | ✓ aligned | Phase 4 |
| `public bool BigEndian { get; set; }` | `PARAMDEF.cs:26` | property | `sf_paramdef_get_big_endian` | ✓ aligned | Phase 4 |
| `public bool Unicode { get; set; }` | `PARAMDEF.cs:31` | property | `sf_paramdef_get_unicode` | ✓ aligned | Phase 4 |
| `public short FormatVersion { get; set; }` | `PARAMDEF.cs:45` | property | `sf_paramdef_get_format_version` | ✓ aligned | Phase 4 |
| `public List<Field> Fields { get; set; }` | `PARAMDEF.cs:55` | property | `sf_paramdef_get_fields` | ✓ aligned | Phase 4 |
| `public bool VersionAware { get; set; }` | `PARAMDEF.cs:61` | property | `sf_paramdef_get_version_aware` | ✓ aligned | Phase 4 |
| `public int GetRowSize(ulong version)` | `PARAMDEF.cs:283` | method | `sf_paramdef_get_row_size` | ✓ aligned | Phase 4 |
| `public static PARAMDEF XmlDeserialize(string path, ...)` | `PARAMDEF.cs:370` | static method | `sf_paramdef_xml_deserialize_from_path` | ✓ aligned | Phase 4 |
| `public static PARAMDEF XmlDeserialize(Stream stream, ...)` | `PARAMDEF.cs:387` | static method | `sf_paramdef_xml_deserialize_from_stream` | ✓ aligned | Phase 4 |
| `public void XmlSerialize(string path, ...)` | `PARAMDEF.cs:397` | method | `sf_paramdef_xml_serialize_to_path` | _skipped_ | Deferred to v1.1 |
| `public void XmlSerialize(string path, int xmlVersion, ...)` | `PARAMDEF.cs:405` | method | `sf_paramdef_xml_serialize_to_path_ex` | _skipped_ | Deferred to v1.1 |
| `public class Field` | `Field.cs:107` | class | `sf_paramdef_field_t` | ✓ aligned | Phase 4 |
| `public string DisplayName { get; set; }` | `Field.cs:117` | property | `sf_paramdef_field_get_display_name` | ✓ aligned | Phase 4 |
| `public DefType DisplayType { get; set; }` | `Field.cs:122` | property | `sf_paramdef_field_get_display_type` | ✓ aligned | Phase 4 |
| `public string InternalName { get; set; }` | `Field.cs:172` | property | `sf_paramdef_field_get_internal_name` | ✓ aligned | Phase 4 |
| `public int BitSize { get; set; }` | `Field.cs:177` | property | `sf_paramdef_field_get_bit_size` | ✓ aligned | Phase 4 |
| `public enum DefType` | `Field.cs:14` | enum | `sf_paramdef_def_type_t` | ✓ aligned | Phase 4 |
| `public enum EditFlags` | `Field.cs:86` | enum | `sf_paramdef_edit_flags_t` | ✓ aligned | Phase 4 |
| `public static int GetValueSize(DefType type)` | `ParamUtil.cs:271` | static method | `sf_param_util_get_value_size` | ✓ aligned | Phase 4 |
| `public static bool IsBitType(DefType type)` | `ParamUtil.cs:239` | static method | `sf_param_util_is_bit_type` | ✓ aligned | Phase 4 |
| `public static int GetBitLimit(DefType type)` | `ParamUtil.cs:335` | static method | `sf_param_util_get_bit_limit` | ✓ aligned | Phase 4 |
| `<Index>` | Paramdex XML | element | `sf_paramdef_get_index` | + extension | Paramdex XML only; binary returns -1 |
| `<SortID>` | Paramdex XML | element | `sf_paramdef_field_get_sort_id` | + extension | Paramdex XML only; binary returns 0 |
