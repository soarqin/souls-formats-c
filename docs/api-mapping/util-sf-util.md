# SFUtil — Row-level Mapping

Upstream: `SoulsFormats/Utilities/SFUtil.cs` at the pinned commit (see
[UPSTREAM.md](UPSTREAM.md)).

The upstream `SFUtil` class is a grab-bag of three static helpers; only the
DCX-aware reader entry point survives the C port. The other two are
.NET-LINQ helpers that have no idiomatic C equivalent (see [POLICY.md §9 —
internal upstream APIs / non-idiomatic C wrappers](POLICY.md)).

| Upstream signature | Upstream loc (SFUtil.cs:LINE) | Kind | Our API | Status | Notes |
|---|---|---|---|---|---|
| `GetDecompressedBinaryReader(BinaryReaderEx br, out DCX.CompressionInfo compression)` | SFUtil.cs:13-25 | static method | `sf_get_decompressed_reader` (declared in `sf_io.h`, defined in `src/core/sf_util.c`) | ✓ aligned | Borrow vs new-allocation paths distinguished by `out_info->type`. New reader bundles ownership of decompressed buffer + internal memory istream; single `sf_binary_reader_destroy(*out_reader)` cleans both. |
| `ConcatAll<T>(params IEnumerable<T>[] lists)` | SFUtil.cs:30-36 | static method (generic) | _skipped_ | _skipped_ | .NET-LINQ helper; C consumers use `memcpy` / `kvec_t`-style growth; not a C idiom per [POLICY.md §9](POLICY.md). |
| `Dictionize<T>(List<T> items)` | SFUtil.cs:41-47 | static method (generic) | _skipped_ | _skipped_ | .NET helper to build `Dictionary<int,T>`; C consumers index the array directly or use `klib khash_t`; not a C idiom per [POLICY.md §9](POLICY.md). |
