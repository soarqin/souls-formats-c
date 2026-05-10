# FMG API Mapping

Upstream reference: `SoulsFormats/Formats/FMG.cs`

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public class FMG` | `FMG.cs:11` | class | `sf_fmg_t` | ✓ aligned | Phase 4 |
| `public List<Entry> Entries { get; set; }` | `FMG.cs:16` | property | `sf_fmg_get_entries` | ✓ aligned | Phase 4 |
| `public FMGVersion Version { get; set; }` | `FMG.cs:21` | property | `sf_fmg_get_version` | ✓ aligned | Phase 4 |
| `public bool BigEndian { get; set; }` | `FMG.cs:26` | property | `sf_fmg_get_big_endian` | ✓ aligned | Phase 4 |
| `public bool Unicode { get; set; }` | `FMG.cs:31` | property | `sf_fmg_get_unicode` | ✓ aligned | Phase 4 |
| `public bool Md5 { get; set; }` | `FMG.cs:36` | property | `sf_fmg_get_md5` | _skipped_ | Upstream doesn't verify, mirror |
| `public bool ReuseOffsets { get; set; }` | `FMG.cs:41` | property | `sf_fmg_get_reuse_offsets` | ✓ aligned | Phase 4 |
| `public class Entry` | `FMG.cs:301` | class | `sf_fmg_entry_t` | ✓ aligned | Phase 4 |
| `public int ID { get; set; }` | `FMG.cs:306` | property | `sf_fmg_entry_get_id` | ✓ aligned | Phase 4 |
| `public string Text { get; set; }` | `FMG.cs:311` | property | `sf_fmg_entry_get_text` | ✓ aligned | Phase 4 |
| `public enum FMGVersion` | `FMG.cs:334` | enum | `sf_fmg_version_t` | ✓ aligned | Phase 4 |
