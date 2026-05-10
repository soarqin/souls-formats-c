# PathHelper — API Mapping

**Upstream file**: `SoulsFormats/Utilities/IO/PathHelper.cs`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|---|---|---|---|---|---|
| `static string Backup(string file, bool overwrite = false)` | `PathHelper.cs:13` | static method | `sf_path_backup(utf8_path, overwrite, out_backup_path, alloc)` | `✓ aligned` | Default overwrite=false documented in header; Win32 CopyFileW |
| `static string GetRealExtension(string path)` | `PathHelper.cs:24` | static method | `sf_path_get_real_extension(utf8_path, out_ext, alloc)` | `✓ aligned` | Pure string ops; .dcx → inner extension |
| `static string GetRealFileName(string path)` | `PathHelper.cs:35` | static method | `sf_path_get_real_file_name(utf8_path, out_name, alloc)` | `✓ aligned` | Pure string ops; strips .dcx + inner extension |
