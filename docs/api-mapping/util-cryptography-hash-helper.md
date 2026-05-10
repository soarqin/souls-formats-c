# HashHelper — API Mapping

**Upstream files**: `SoulsFormats/Utilities/Cryptography/HashHelper.cs`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|---|---|---|---|---|---|
| `static uint FromPathHash(string text)` | `HashHelper.cs:13` | static method | `sf_path_hash(const char *utf8_path) → uint32_t` | `✓ aligned` | ASCII-only fold; UTF-8 bytes outside ASCII pass through unchanged, matching upstream `ToLowerInvariant` behaviour |
| `static bool IsPrime(uint candidate)` | `HashHelper.cs:24` | static method | `sf_is_prime(uint32_t candidate) → bool` | `✓ aligned` | Trial-division; exact algorithm match including boundary cases (0,1→false; 2→true; even→false) |
