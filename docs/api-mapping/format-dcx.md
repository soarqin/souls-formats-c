# DCX API Mapping

Upstream reference: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/DCX.cs`.

Policy notes: C overloads use explicit `_from_buffer`, `_from_stream`, `_from_path`,
`_to_buffer`, `_to_stream`, and `_to_path` suffixes. C# exceptions map to `sf_result_t`,
`out` parameters map to pointer parameters, and returned byte arrays are caller-owned buffers
freed with `sf_free()`.

| Upstream signature | Upstream loc (DCX.cs:LINE) | Kind | Our API (or 未实现) | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public static bool Is(Stream stream)` | `DCX.cs:28` | static method | `sf_dcx_is_from_stream` | ✓ aligned | Requires stream position 0, mirroring upstream safety check. |
| `public static bool Is(byte[] bytes)` | `DCX.cs:47` | static method | `sf_dcx_is_from_buffer` | ✓ aligned | Uses magic sniff without consuming caller data. |
| `public static bool Is(string path)` | `DCX.cs:58` | static method | `sf_dcx_is_from_path` | ✓ aligned | UTF-8 path boundary; internal stream uses Win32. |
| `public static byte[] Decompress(Stream stream, out CompressionInfo compression)` | `DCX.cs:74` | static method | `sf_dcx_decompress_from_stream` | ✓ aligned | `compression` maps to optional `sf_dcx_compression_info_t *out_info`. |
| `public static byte[] Decompress(Stream stream)` | `DCX.cs:91` | static method | `sf_dcx_decompress_from_stream(..., NULL, ...)` | ✓ aligned | C caller passes `NULL` for `out_info`. |
| `public static byte[] Decompress(byte[] data, out CompressionInfo compression)` | `DCX.cs:99` | static method | `sf_dcx_decompress_from_buffer` | ✓ aligned | Also available as legacy-named `sf_dcx_decompress`. |
| `public static byte[] Decompress(byte[] data)` | `DCX.cs:110` | static method | `sf_dcx_decompress_from_buffer(..., NULL, ...)` | ✓ aligned | C caller passes `NULL` for `out_info`. |
| `public static byte[] Decompress(string path, out CompressionInfo compression)` | `DCX.cs:118` | static method | `sf_dcx_decompress_from_path` | ✓ aligned | UTF-8 path boundary; output buffer uses provided allocator. |
| `public static byte[] Decompress(string path)` | `DCX.cs:130` | static method | `sf_dcx_decompress_from_path(..., NULL, ...)` | ✓ aligned | C caller passes `NULL` for `out_info`. |
| `public static byte[] Compress(byte[] data, CompressionInfo compression)` | `DCX.cs:482` | static method | `sf_dcx_compress_to_buffer` | ✓ aligned | Also available as legacy-named `sf_dcx_compress_ex`. |
| `public static void Compress(byte[] data, CompressionInfo compression, string path)` | `DCX.cs:492` | static method | `sf_dcx_compress_to_path` | ✓ aligned | UTF-8 path boundary; internal stream uses Win32. |
| `public enum Type` | `DCX.cs:865` | enum | `sf_dcx_type_t` | ✓ aligned | Values keep upstream order with `SF_DCX_TYPE_` prefix. |
| `Type.Unknown` | `DCX.cs:870` | enum value | `SF_DCX_TYPE_UNKNOWN` | ✓ aligned | Undetected / unsupported compression wrapper. |
| `Type.None` | `DCX.cs:875` | enum value | `SF_DCX_TYPE_NONE` | ✓ aligned | Uncompressed payload copy. |
| `Type.Zlib` | `DCX.cs:880` | enum value | `SF_DCX_TYPE_ZLIB` | ✓ aligned | Plain zlib-wrapped data. |
| `Type.DCP_EDGE` | `DCX.cs:885` | enum value | `SF_DCX_TYPE_DCP_EDGE` | ✓ aligned | DCP header, chunked raw deflate. |
| `Type.DCP_DFLT` | `DCX.cs:890` | enum value | `SF_DCX_TYPE_DCP_DFLT` | ✓ aligned | DCP header, zlib deflate. |
| `Type.DCX_EDGE` | `DCX.cs:895` | enum value | `SF_DCX_TYPE_DCX_EDGE` | ✓ aligned | DCX header, chunked raw deflate. |
| `Type.DCX_DFLT` | `DCX.cs:900` | enum value | `SF_DCX_TYPE_DCX_DFLT` | ✓ aligned | DCX header, zlib deflate with DFLT parameters. |
| `Type.DCX_KRAK` | `DCX.cs:905` | enum value | `SF_DCX_TYPE_DCX_KRAK` | ✓ aligned | DCX header, Oodle KRAK. |
| `Type.DCX_ZSTD` | `DCX.cs:910` | enum value | `SF_DCX_TYPE_DCX_ZSTD` | ✓ aligned | DCX header, Zstandard. |
| `public enum DefaultType` | `DCX.cs:916` | enum | `sf_dcx_default_type_t` | ✓ aligned | Cast-to-`Type` upstream behavior maps to factory helper in C. |
| `DefaultType.DemonsSouls` | `DCX.cs:921` | enum value | `SF_DCX_DEFAULT_TYPE_DEMONS_SOULS` | ✓ aligned | Produces `DCX_EDGE`. |
| `DefaultType.DarkSouls1` | `DCX.cs:926` | enum value | `SF_DCX_DEFAULT_TYPE_DARK_SOULS_1` | ✓ aligned | Produces `DCX_DFLT_10000_24_9`. |
| `DefaultType.DarkSouls2` | `DCX.cs:931` | enum value | `SF_DCX_DEFAULT_TYPE_DARK_SOULS_2` | ✓ aligned | Produces `DCX_DFLT_10000_24_9`. |
| `DefaultType.Bloodborne` | `DCX.cs:936` | enum value | `SF_DCX_DEFAULT_TYPE_BLOODBORNE` | ✓ aligned | Produces `DCX_DFLT_10000_44_9`. |
| `DefaultType.DarkSouls3` | `DCX.cs:941` | enum value | `SF_DCX_DEFAULT_TYPE_DARK_SOULS_3` | ✓ aligned | Produces `DCX_DFLT_10000_44_9`. |
| `DefaultType.Sekiro` | `DCX.cs:946` | enum value | `SF_DCX_DEFAULT_TYPE_SEKIRO` | ✓ aligned | Produces Elden Ring KRAK preset (`level=6`, Kraken). |
| `DefaultType.EldenRing` | `DCX.cs:951` | enum value | `SF_DCX_DEFAULT_TYPE_ELDEN_RING` | ✓ aligned | Produces Elden Ring KRAK preset (`level=6`, Kraken). |
| `DefaultType.AC6` | `DCX.cs:956` | enum value | `SF_DCX_DEFAULT_TYPE_AC6` | ✓ aligned | Produces Armored Core VI KRAK preset (`level=9`, Kraken). |
| `public interface CompressionInfo` | `DCX.cs:959` | interface | `sf_dcx_compression_info_t` | ✓ aligned | C tagged union replaces polymorphic value structs. |
| `CompressionInfo.Type { get; }` | `DCX.cs:962` | property | `sf_dcx_compression_info_t.type` | ✓ aligned | Public discriminator. |
| `public struct UnkCompressionInfo` | `DCX.cs:966` | CompressionInfo variant | `sf_dcx_unk_info_t` | ✓ aligned | Empty value struct in union arm. |
| `UnkCompressionInfo.Type => Type.Unknown` | `DCX.cs:969` | property | `SF_DCX_TYPE_UNKNOWN` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public struct NoCompressionInfo` | `DCX.cs:973` | CompressionInfo variant | `sf_dcx_none_info_t` | ✓ aligned | Empty value struct in union arm. |
| `NoCompressionInfo.Type => Type.None` | `DCX.cs:976` | property | `SF_DCX_TYPE_NONE` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public struct DcpDfltCompressionInfo` | `DCX.cs:980` | CompressionInfo variant | `sf_dcx_dcp_dflt_info_t` | ✓ aligned | Empty value struct in union arm. |
| `DcpDfltCompressionInfo.Type => Type.DCP_DFLT` | `DCX.cs:983` | property | `SF_DCX_TYPE_DCP_DFLT` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public struct DcpEdgeCompressionInfo` | `DCX.cs:987` | CompressionInfo variant | `sf_dcx_dcp_edge_info_t` | ✓ aligned | Empty value struct in union arm. |
| `DcpEdgeCompressionInfo.Type => Type.DCP_EDGE` | `DCX.cs:990` | property | `SF_DCX_TYPE_DCP_EDGE` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public struct ZlibCompressionInfo` | `DCX.cs:994` | CompressionInfo variant | `sf_dcx_zlib_info_t` | ✓ aligned | Empty value struct in union arm. |
| `ZlibCompressionInfo.Type => Type.Zlib` | `DCX.cs:997` | property | `SF_DCX_TYPE_ZLIB` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public struct DcxEdgeCompressionInfo` | `DCX.cs:1001` | CompressionInfo variant | `sf_dcx_dcx_edge_info_t` | ✓ aligned | Empty value struct in union arm. |
| `DcxEdgeCompressionInfo.Type => Type.DCX_EDGE` | `DCX.cs:1004` | property | `SF_DCX_TYPE_DCX_EDGE` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `public enum DfltCompressionPreset` | `DCX.cs:1007` | enum | `sf_dcx_dflt_compression_preset_t` | ✓ aligned | Factory maps preset to `sf_dcx_dcx_dflt_info_t`. |
| `DfltCompressionPreset.DCX_DFLT_10000_24_9` | `DCX.cs:1009` | enum value | `SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_24_9` | ✓ aligned | `unk04=0x10000`, `unk10=0x24`, `unk14=0x2C`, `unk30=9`, `unk38=0`. |
| `DfltCompressionPreset.DCX_DFLT_10000_44_9` | `DCX.cs:1010` | enum value | `SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_44_9` | ✓ aligned | `unk04=0x10000`, `unk10=0x44`, `unk14=0x4C`, `unk30=9`, `unk38=0`. |
| `DfltCompressionPreset.DCX_DFLT_11000_44_8` | `DCX.cs:1011` | enum value | `SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_8` | ✓ aligned | `unk04=0x11000`, `unk10=0x44`, `unk14=0x4C`, `unk30=8`, `unk38=0`. |
| `DfltCompressionPreset.DCX_DFLT_11000_44_9` | `DCX.cs:1012` | enum value | `SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9` | ✓ aligned | `unk04=0x11000`, `unk10=0x44`, `unk14=0x4C`, `unk30=9`, `unk38=0`. |
| `DfltCompressionPreset.DCX_DFLT_11000_44_9_15` | `DCX.cs:1013` | enum value | `SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9_15` | ✓ aligned | `unk04=0x11000`, `unk10=0x44`, `unk14=0x4C`, `unk30=9`, `unk38=15`. |
| `public struct DcxDfltCompressionInfo` | `DCX.cs:1016` | CompressionInfo variant | `sf_dcx_dcx_dflt_info_t` | ✓ aligned | Stores all five upstream fields. |
| `DcxDfltCompressionInfo.Type => Type.DCX_DFLT` | `DCX.cs:1019` | property | `SF_DCX_TYPE_DCX_DFLT` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `DcxDfltCompressionInfo.Unk04 { get; }` | `DCX.cs:1021` | property | `sf_dcx_dcx_dflt_info_t.unk04` | ✓ aligned | Signed 32-bit to mirror upstream `int`. |
| `DcxDfltCompressionInfo.Unk10 { get; }` | `DCX.cs:1023` | property | `sf_dcx_dcx_dflt_info_t.unk10` | ✓ aligned | Signed 32-bit to mirror upstream `int`. |
| `DcxDfltCompressionInfo.Unk14 { get; }` | `DCX.cs:1025` | property | `sf_dcx_dcx_dflt_info_t.unk14` | ✓ aligned | Signed 32-bit to mirror upstream `int`. |
| `DcxDfltCompressionInfo.Unk30 { get; }` | `DCX.cs:1027` | property | `sf_dcx_dcx_dflt_info_t.unk30` | ✓ aligned | Compression-level byte in DCP block. |
| `DcxDfltCompressionInfo.Unk38 { get; }` | `DCX.cs:1029` | property | `sf_dcx_dcx_dflt_info_t.unk38` | ✓ aligned | Additional byte in DCP block. |
| `DcxDfltCompressionInfo(int unk04, int unk10, int unk14, byte unk30, byte unk38)` | `DCX.cs:1031` | constructor | direct `sf_dcx_compression_info_t` value initialization | ✓ aligned | C value type has no allocation or constructor symbol. |
| `DcxDfltCompressionInfo(DfltCompressionPreset preset)` | `DCX.cs:1040` | constructor | `sf_dcx_compression_info_from_dflt_preset` | ✓ aligned | Invalid preset maps to `SF_ERR_INVALID_ARG`. |
| `public enum KrakCompressionPreset` | `DCX.cs:1085` | enum | `sf_dcx_krak_compression_preset_t` | ✓ aligned | Factory maps preset to `sf_dcx_dcx_krak_info_t`. |
| `KrakCompressionPreset.EldenRing` | `DCX.cs:1087` | enum value | `SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING` | ✓ aligned | `compression_level=6`, `oodle_compressor_type=Kraken`. |
| `KrakCompressionPreset.ArmoredCore6` | `DCX.cs:1088` | enum value | `SF_DCX_KRAK_COMPRESSION_PRESET_ARMORED_CORE_6` | ✓ aligned | `compression_level=9`, `oodle_compressor_type=Kraken`. |
| `public struct DcxKrakCompressionInfo` | `DCX.cs:1091` | CompressionInfo variant | `sf_dcx_dcx_krak_info_t` | ✓ aligned | Stores compression level and Oodle compressor type. |
| `DcxKrakCompressionInfo.Type => Type.DCX_KRAK` | `DCX.cs:1094` | property | `SF_DCX_TYPE_DCX_KRAK` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `DcxKrakCompressionInfo.CompressionLevel { get; }` | `DCX.cs:1097` | property | `sf_dcx_dcx_krak_info_t.compression_level` | ✓ aligned | Byte level passed to Oodle. |
| `DcxKrakCompressionInfo.OodleCompressorType { get; }` | `DCX.cs:1100` | property | `sf_dcx_dcx_krak_info_t.oodle_compressor_type` | ✓ aligned | Upstream default is `OodleLZ_Compressor_Kraken`. |
| `DcxKrakCompressionInfo(byte compressionLevel, OodleLZ_Compressor oodleCompressorType = Kraken)` | `DCX.cs:1102` | constructor | direct `sf_dcx_compression_info_t` value initialization | ✓ aligned | C caller supplies both fields explicitly; default noted here. |
| `DcxKrakCompressionInfo(KrakCompressionPreset preset)` | `DCX.cs:1109` | constructor | `sf_dcx_compression_info_from_krak_preset` | ✓ aligned | Invalid preset maps to `SF_ERR_INVALID_ARG`. |
| `public struct DcxZstdCompressionInfo` | `DCX.cs:1128` | CompressionInfo variant | `sf_dcx_dcx_zstd_info_t` | ✓ aligned | Stores compression level. |
| `DcxZstdCompressionInfo.Type => Type.DCX_ZSTD` | `DCX.cs:1131` | property | `SF_DCX_TYPE_DCX_ZSTD` | ✓ aligned | Stored in `sf_dcx_compression_info_t.type`. |
| `DcxZstdCompressionInfo.CompressionLevel { get; }` | `DCX.cs:1134` | property | `sf_dcx_dcx_zstd_info_t.compression_level` | ✓ aligned | Byte level passed to zstd. |
| `DcxZstdCompressionInfo(byte compressionLevel)` | `DCX.cs:1136` | constructor | direct `sf_dcx_compression_info_t` value initialization | ✓ aligned | C value type has no allocation or constructor symbol. |
