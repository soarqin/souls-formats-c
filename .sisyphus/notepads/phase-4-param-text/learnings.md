2026-05-11: PARAMTDF only supports six integer value types (s8/u8/s16/u16/s32/u32); upstream rejects all wider/other field kinds.
2026-05-11: PARAMTDF entry names may be NULL and should be preserved as valid state in the public API.
2026-05-11: For new public headers, `__has_include` fallbacks can keep both compiler syntax-checks and header-local IntelliSense happy without changing the exported API.
2026-05-11: Public headers under `include/souls_formats/` should prefer local quoted includes (`"sf_common.h"`) so standalone syntax checks and clangd both resolve the include chain cleanly.
2026-05-11: For header-only additions, a conditional `__has_include` fallback to the absolute workspace path can preserve the required public include form while making clangd diagnostics pass.
