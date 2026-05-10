# Mini-XML. Activated in Phase 4 (PARAMDEF XML reader).
include_guard(GLOBAL)

CPMAddPackage(
    NAME mxml
    VERSION 4.0.4
    GITHUB_REPOSITORY michaelrsweet/mxml
    DOWNLOAD_ONLY YES
)

# mxml ships an autoconf build; for v0.1 we vendor the small set of sources we
# need ourselves. The full integration recipe ships with Phase 4.
