# PARAMTDF API Mapping

Upstream reference: `SoulsFormats/Formats/PARAM/PARAMTDF.cs`

| Upstream signature | Upstream loc | Kind | Our API (or 未实现) | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public class PARAMTDF` | `PARAMTDF.cs:10` | class | `sf_paramtdf_t` | 未实现 | Phase 4 |
| `public string Name { get; set; }` | `PARAMTDF.cs:15` | property | `sf_paramtdf_get_name` | 未实现 | Phase 4 |
| `public PARAMDEF.DefType Type { get; set; }` | `PARAMTDF.cs:20` | property | `sf_paramtdf_get_type` | 未实现 | Phase 4 |
| `public List<Entry> Entries { get; set; }` | `PARAMTDF.cs:37` | property | `sf_paramtdf_get_entries` | 未实现 | Phase 4 |
| `public PARAMTDF(string text)` | `PARAMTDF.cs:62` | constructor | `sf_paramtdf_from_text` | 未实现 | Phase 4 |
| `public string Write()` | `PARAMTDF.cs:97` | method | `sf_paramtdf_to_text` | 未实现 | Phase 4 |
| `public class Entry` | `PARAMTDF.cs:122` | class | `sf_paramtdf_entry_t` | 未实现 | Phase 4 |
| `public string Name { get; set; }` | `PARAMTDF.cs:127` | property | `sf_paramtdf_entry_get_name` | 未实现 | Phase 4 |
| `public object Value { get; set; }` | `PARAMTDF.cs:132` | property | `sf_paramtdf_entry_get_value` | 未实现 | Phase 4 |
| `public Entry(string name, object value)` | `PARAMTDF.cs:137` | constructor | `sf_paramtdf_entry_create` | 未实现 | Phase 4 |
