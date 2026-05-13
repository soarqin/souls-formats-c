# SFEncoding — API Mapping

**Upstream file**: `SoulsFormats/Utilities/Text/SFEncoding.cs`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

## C-style adaptation note

Upstream exposes 4 `static readonly System.Text.Encoding` singletons (ASCII, ShiftJIS, UTF16, UTF16BE). C has no encoding-object idiom; we expose imperative converter functions instead. This is a documented C-style adaptation per `docs/api-mapping/POLICY.md §11 (Encoding boundary)`.

| Upstream field | Upstream loc | Kind | Our API | Status | Notes |
|---|---|---|---|---|---|
| `static readonly Encoding ASCII` | `SFEncoding.cs:13` | static field | `sf_ascii_to_utf8` / `sf_utf8_to_ascii` | `~ partial` | C-style adaptation: imperative functions instead of singleton; non-ASCII bytes decode as `?` to match .NET ASCII replacement behavior |
| `static readonly Encoding ShiftJIS` | `SFEncoding.cs:18` | static field | `sf_shift_jis_to_utf8` / `sf_utf8_to_shift_jis` | `~ partial` | Win32 CP932; round-trip identity tested |
| `static readonly Encoding UTF16` | `SFEncoding.cs:23` | static field | `sf_utf16le_to_utf8` / `sf_utf8_to_utf16le` | `~ partial` | Little-endian; round-trip identity tested |
| `static readonly Encoding UTF16BE` | `SFEncoding.cs:28` | static field | `sf_utf16be_to_utf8` / `sf_utf8_to_utf16be` | `~ partial` | Big-endian (byte-swap on Win32); round-trip identity tested |
