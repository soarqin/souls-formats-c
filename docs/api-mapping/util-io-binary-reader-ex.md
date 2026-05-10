# BinaryReaderEx API Mapping

Upstream reference: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryReaderEx.cs` at pinned commit from `UPSTREAM.md`.

| Upstream signature | Upstream loc (BinaryReaderEx.cs:LINE) | Kind | Our API (or 未实现) | Status | Notes |
|---|---|---|---|---|---|
| `public static bool IsFlexible { get; set; }` getter | BinaryReaderEx.cs:22 | Static property getter | `sf_binary_reader_flexible_default()` | ✓ aligned | Static default exposed separately; readers copy this value on create. |
| `public static bool IsFlexible { get; set; }` setter | BinaryReaderEx.cs:22 | Static property setter | `sf_binary_reader_set_flexible_default(bool flexible)` | ✓ aligned | Global default for newly-created readers. |
| `public bool IsDisposed { get; private set; }` getter | BinaryReaderEx.cs:30 | Property getter | `_skipped_` | _skipped_ | C object lifetime is explicit; using a destroyed opaque pointer is invalid. |
| `public bool BigEndian { get; set; }` getter | BinaryReaderEx.cs:35 | Property getter | `sf_binary_reader_big_endian(const sf_binary_reader_t*)` | ✓ aligned | Returns false for NULL defensively. |
| `public bool BigEndian { get; set; }` setter | BinaryReaderEx.cs:35 | Property setter | `sf_binary_reader_set_big_endian(sf_binary_reader_t*, bool)` | ✓ aligned | Mutates per-reader endian state. |
| `public bool VarintLong { get; set; }` getter | BinaryReaderEx.cs:40 | Property getter | `sf_binary_reader_varint_long(const sf_binary_reader_t*)` | ✓ aligned | false means 4-byte varint. |
| `public bool VarintLong { get; set; }` setter | BinaryReaderEx.cs:40 | Property setter | `sf_binary_reader_set_varint_long(sf_binary_reader_t*, bool)` | ✓ aligned | true means 8-byte varint. |
| `public int VarintSize => VarintLong ? 8 : 4` | BinaryReaderEx.cs:45 | Property getter | `sf_binary_reader_varint_long()` | ~ partial | Size is derived by callers as 8 or 4; no dedicated getter yet. |
| `public Stream Stream { get; }` | BinaryReaderEx.cs:50 | Property getter | `sf_binary_reader_stream(sf_binary_reader_t*)` | ✓ aligned | Borrowed stream accessor. |
| `public long Position get` | BinaryReaderEx.cs:57 | Property getter | `sf_binary_reader_position(const sf_binary_reader_t*)` | ✓ aligned | Reads underlying stream position. |
| `public long Position set` | BinaryReaderEx.cs:58 | Property setter | `sf_istream_seek(sf_binary_reader_stream(r), pos)` | ~ partial | Position setter is provided by stream seek rather than reader wrapper. |
| `public long Length => Stream.Length` | BinaryReaderEx.cs:64 | Property getter | `sf_binary_reader_length(const sf_binary_reader_t*)` | ✓ aligned | Reads underlying stream length. |
| `public long Remaining => Stream.Length - Stream.Position` | BinaryReaderEx.cs:69 | Property getter | `sf_binary_reader_remaining(const sf_binary_reader_t*)` | ✓ aligned | Reads underlying stream remaining byte count. |
| `public BinaryReaderEx(bool bigEndian, string path)` | BinaryReaderEx.cs:74 | Constructor | `sf_istream_open_file` + `sf_binary_reader_create` | ✓ aligned | Constructor split into stream open plus reader creation. |
| `public BinaryReaderEx(bool bigEndian, byte[] input)` | BinaryReaderEx.cs:79 | Constructor | `sf_istream_open_memory` + `sf_binary_reader_create` | ✓ aligned | Caller-owned memory stream mirrors byte-array source. |
| `public BinaryReaderEx(bool bigEndian, Stream stream, bool leaveOpen = false)` | BinaryReaderEx.cs:84 | Constructor | `sf_binary_reader_create` | ✓ aligned | C reader always borrows stream; caller closes separately. |
| `public void StepIn(long offset)` | BinaryReaderEx.cs:143 | Method | `sf_binary_reader_step_in(sf_binary_reader_t*, int64_t pos)` | ✓ aligned | Pushes current offset and seeks absolute position. |
| `public void StepOut()` | BinaryReaderEx.cs:152 | Method | `sf_binary_reader_step_out(sf_binary_reader_t*)` | ✓ aligned | Returns `SF_ERR_INTERNAL` if already fully stepped out. |
| `public void Pad(int align)` | BinaryReaderEx.cs:163 | Method | `sf_binary_reader_pad(sf_binary_reader_t*, int align)` | ✓ aligned | Rejects non-positive alignment. |
| `public void PadRelative(long start, int align)` | BinaryReaderEx.cs:172 | Method | `sf_binary_reader_pad_relative(sf_binary_reader_t*, int64_t start, int align)` | ✓ aligned | Rejects non-positive alignment. |
| `public void Skip(int count)` | BinaryReaderEx.cs:182 | Method | `sf_binary_reader_skip(sf_binary_reader_t*, int64_t n)` | ✓ aligned | Allows signed relative movement through stream seek. |
| `public long GetNextPaddedOffsetAfterCurrentField(int currentFieldLength, int align)` | BinaryReaderEx.cs:188 | Method | 未实现 | 未实现 | Planned helper; no current C API wrapper. |
| `public bool ReadBoolean()` | BinaryReaderEx.cs:203 | Method | `sf_binary_reader_read_bool(sf_binary_reader_t*, bool*)` | ✓ aligned | Only `0` and `1` accepted; other bytes return `SF_ERR_BAD_MAGIC`. |
| `public bool[] ReadBooleans(int count)` | BinaryReaderEx.cs:218 | Method | `sf_binary_reader_read_bools(sf_binary_reader_t*, size_t n, bool*)` | ✓ aligned | Caller supplies output array. |
| `public bool GetBoolean(long offset)` | BinaryReaderEx.cs:229 | Method | `sf_binary_reader_get_bool(sf_binary_reader_t*, int64_t off, bool*)` | ✓ aligned | Restores original stream cursor. |
| `public bool[] GetBooleans(long offset, int count)` | BinaryReaderEx.cs:237 | Method | `sf_binary_reader_get_bools(sf_binary_reader_t*, int64_t off, size_t n, bool*)` | ✓ aligned | Restores original stream cursor. |
| `public bool AssertBoolean(bool option)` | BinaryReaderEx.cs:245 | Method | `sf_binary_reader_assert_bool` / `sf_binary_reader_assert_bool_one` | ✓ aligned | Multi-option C API plus single-option convenience. |
| `public sbyte ReadSByte()` | BinaryReaderEx.cs:255 | Method | `sf_binary_reader_read_i8(sf_binary_reader_t*, int8_t*)` | ✓ aligned | C uses fixed-width type spelling. |
| `public sbyte[] ReadSBytes(int count)` | BinaryReaderEx.cs:263 | Method | `sf_binary_reader_read_i8s(sf_binary_reader_t*, size_t n, int8_t*)` | ✓ aligned | Caller supplies output array. |
| `public sbyte GetSByte(long offset)` | BinaryReaderEx.cs:274 | Method | `sf_binary_reader_get_i8(sf_binary_reader_t*, int64_t off, int8_t*)` | ✓ aligned | Restores original stream cursor. |
| `public sbyte[] GetSBytes(long offset, int count)` | BinaryReaderEx.cs:282 | Method | `sf_binary_reader_get_i8s(sf_binary_reader_t*, int64_t off, size_t n, int8_t*)` | ✓ aligned | Restores original stream cursor. |
| `public sbyte AssertSByte(params sbyte[] options)` | BinaryReaderEx.cs:290 | Method | `sf_binary_reader_assert_i8` / `sf_binary_reader_assert_i8_one` | ✓ aligned | `params` maps to `(size_t, const int8_t*)`. |
| `public byte ReadByte()` | BinaryReaderEx.cs:300 | Method | `sf_binary_reader_read_u8(sf_binary_reader_t*, uint8_t*)` | ✓ aligned | C uses fixed-width type spelling. |
| `public byte[] ReadBytes(int count)` | BinaryReaderEx.cs:308 | Method | `sf_binary_reader_read_u8s` / `sf_binary_reader_read_bytes` | ✓ aligned | Byte plural aliases raw byte read. |
| `public void ReadBytes(byte[] buffer, int index, int count)` | BinaryReaderEx.cs:319 | Method | `sf_binary_reader_read_bytes(sf_binary_reader_t*, void*, size_t)` | ~ partial | C caller passes pointer to destination start instead of buffer+index. |
| `public byte GetByte(long offset)` | BinaryReaderEx.cs:329 | Method | `sf_binary_reader_get_u8(sf_binary_reader_t*, int64_t off, uint8_t*)` | ✓ aligned | Restores original stream cursor. |
| `public byte[] GetBytes(long offset, int count)` | BinaryReaderEx.cs:337 | Method | `sf_binary_reader_get_u8s` / `sf_binary_reader_get_bytes` | ✓ aligned | Caller supplies output buffer. |
| `public void GetBytes(long offset, byte[] buffer, int index, int count)` | BinaryReaderEx.cs:348 | Method | `sf_binary_reader_get_bytes(sf_binary_reader_t*, int64_t off, void*, size_t)` | ~ partial | C caller passes pointer to destination start instead of buffer+index. |
| `public byte AssertByte(params byte[] options)` | BinaryReaderEx.cs:358 | Method | `sf_binary_reader_assert_u8` / `sf_binary_reader_assert_u8_one` | ✓ aligned | `params` maps to `(size_t, const uint8_t*)`. |
| `public short ReadInt16()` | BinaryReaderEx.cs:368 | Method | `sf_binary_reader_read_i16(sf_binary_reader_t*, int16_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public short[] ReadInt16s(int count)` | BinaryReaderEx.cs:379 | Method | `sf_binary_reader_read_i16s(sf_binary_reader_t*, size_t n, int16_t*)` | ✓ aligned | Caller supplies output array. |
| `public short GetInt16(long offset)` | BinaryReaderEx.cs:390 | Method | `sf_binary_reader_get_i16(sf_binary_reader_t*, int64_t off, int16_t*)` | ✓ aligned | Restores original stream cursor. |
| `public short[] GetInt16s(long offset, int count)` | BinaryReaderEx.cs:398 | Method | `sf_binary_reader_get_i16s(sf_binary_reader_t*, int64_t off, size_t n, int16_t*)` | ✓ aligned | Restores original stream cursor. |
| `public short AssertInt16(params short[] options)` | BinaryReaderEx.cs:406 | Method | `sf_binary_reader_assert_i16` / `sf_binary_reader_assert_i16_one` | ✓ aligned | `params` maps to `(size_t, const int16_t*)`. |
| `public ushort ReadUInt16()` | BinaryReaderEx.cs:416 | Method | `sf_binary_reader_read_u16(sf_binary_reader_t*, uint16_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public ushort[] ReadUInt16s(int count)` | BinaryReaderEx.cs:427 | Method | `sf_binary_reader_read_u16s(sf_binary_reader_t*, size_t n, uint16_t*)` | ✓ aligned | Caller supplies output array. |
| `public ushort GetUInt16(long offset)` | BinaryReaderEx.cs:438 | Method | `sf_binary_reader_get_u16(sf_binary_reader_t*, int64_t off, uint16_t*)` | ✓ aligned | Restores original stream cursor. |
| `public ushort[] GetUInt16s(long offset, int count)` | BinaryReaderEx.cs:446 | Method | `sf_binary_reader_get_u16s(sf_binary_reader_t*, int64_t off, size_t n, uint16_t*)` | ✓ aligned | Restores original stream cursor. |
| `public ushort AssertUInt16(params ushort[] options)` | BinaryReaderEx.cs:454 | Method | `sf_binary_reader_assert_u16` / `sf_binary_reader_assert_u16_one` | ✓ aligned | `params` maps to `(size_t, const uint16_t*)`. |
| `public int ReadInt32()` | BinaryReaderEx.cs:464 | Method | `sf_binary_reader_read_i32(sf_binary_reader_t*, int32_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public int[] ReadInt32s(int count)` | BinaryReaderEx.cs:475 | Method | `sf_binary_reader_read_i32s(sf_binary_reader_t*, size_t n, int32_t*)` | ✓ aligned | Caller supplies output array. |
| `public int GetInt32(long offset)` | BinaryReaderEx.cs:486 | Method | `sf_binary_reader_get_i32(sf_binary_reader_t*, int64_t off, int32_t*)` | ✓ aligned | Restores original stream cursor. |
| `public int[] GetInt32s(long offset, int count)` | BinaryReaderEx.cs:494 | Method | `sf_binary_reader_get_i32s(sf_binary_reader_t*, int64_t off, size_t n, int32_t*)` | ✓ aligned | Restores original stream cursor. |
| `public int AssertInt32(params int[] options)` | BinaryReaderEx.cs:502 | Method | `sf_binary_reader_assert_i32` / `sf_binary_reader_assert_i32_one` | ✓ aligned | `params` maps to `(size_t, const int32_t*)`. |
| `public uint ReadUInt32()` | BinaryReaderEx.cs:512 | Method | `sf_binary_reader_read_u32(sf_binary_reader_t*, uint32_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public uint[] ReadUInt32s(int count)` | BinaryReaderEx.cs:523 | Method | `sf_binary_reader_read_u32s(sf_binary_reader_t*, size_t n, uint32_t*)` | ✓ aligned | Caller supplies output array. |
| `public uint GetUInt32(long offset)` | BinaryReaderEx.cs:534 | Method | `sf_binary_reader_get_u32(sf_binary_reader_t*, int64_t off, uint32_t*)` | ✓ aligned | Restores original stream cursor. |
| `public uint[] GetUInt32s(long offset, int count)` | BinaryReaderEx.cs:542 | Method | `sf_binary_reader_get_u32s(sf_binary_reader_t*, int64_t off, size_t n, uint32_t*)` | ✓ aligned | Restores original stream cursor. |
| `public uint AssertUInt32(params uint[] options)` | BinaryReaderEx.cs:550 | Method | `sf_binary_reader_assert_u32` / `sf_binary_reader_assert_u32_one` | ✓ aligned | `params` maps to `(size_t, const uint32_t*)`. |
| `public long ReadInt64()` | BinaryReaderEx.cs:560 | Method | `sf_binary_reader_read_i64(sf_binary_reader_t*, int64_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public long[] ReadInt64s(int count)` | BinaryReaderEx.cs:571 | Method | `sf_binary_reader_read_i64s(sf_binary_reader_t*, size_t n, int64_t*)` | ✓ aligned | Caller supplies output array. |
| `public long GetInt64(long offset)` | BinaryReaderEx.cs:582 | Method | `sf_binary_reader_get_i64(sf_binary_reader_t*, int64_t off, int64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public long[] GetInt64s(long offset, int count)` | BinaryReaderEx.cs:590 | Method | `sf_binary_reader_get_i64s(sf_binary_reader_t*, int64_t off, size_t n, int64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public long AssertInt64(params long[] options)` | BinaryReaderEx.cs:598 | Method | `sf_binary_reader_assert_i64` / `sf_binary_reader_assert_i64_one` | ✓ aligned | `params` maps to `(size_t, const int64_t*)`. |
| `public ulong ReadUInt64()` | BinaryReaderEx.cs:608 | Method | `sf_binary_reader_read_u64(sf_binary_reader_t*, uint64_t*)` | ✓ aligned | Honors `BigEndian`. |
| `public ulong[] ReadUInt64s(int count)` | BinaryReaderEx.cs:619 | Method | `sf_binary_reader_read_u64s(sf_binary_reader_t*, size_t n, uint64_t*)` | ✓ aligned | Caller supplies output array. |
| `public ulong GetUInt64(long offset)` | BinaryReaderEx.cs:630 | Method | `sf_binary_reader_get_u64(sf_binary_reader_t*, int64_t off, uint64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public ulong[] GetUInt64s(long offset, int count)` | BinaryReaderEx.cs:638 | Method | `sf_binary_reader_get_u64s(sf_binary_reader_t*, int64_t off, size_t n, uint64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public ulong AssertUInt64(params ulong[] options)` | BinaryReaderEx.cs:646 | Method | `sf_binary_reader_assert_u64` / `sf_binary_reader_assert_u64_one` | ✓ aligned | `params` maps to `(size_t, const uint64_t*)`. |
| `public long ReadVarint()` | BinaryReaderEx.cs:656 | Method | `sf_binary_reader_read_varint(sf_binary_reader_t*, int64_t*)` | ✓ aligned | Reads i32 or i64 depending on `VarintLong`. |
| `public long[] ReadVarints(int count)` | BinaryReaderEx.cs:667 | Method | `sf_binary_reader_read_varints(sf_binary_reader_t*, size_t n, int64_t*)` | ✓ aligned | Caller supplies output array. |
| `public long GetVarint(long offset)` | BinaryReaderEx.cs:683 | Method | `sf_binary_reader_get_varint(sf_binary_reader_t*, int64_t off, int64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public long[] GetVarints(long offset, int count)` | BinaryReaderEx.cs:694 | Method | `sf_binary_reader_get_varints(sf_binary_reader_t*, int64_t off, size_t n, int64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public long AssertVarint(params long[] options)` | BinaryReaderEx.cs:702 | Method | `sf_binary_reader_assert_varint` / `sf_binary_reader_assert_varint_one` | ✓ aligned | `params` maps to `(size_t, const int64_t*)`. |
| `public float ReadSingle()` | BinaryReaderEx.cs:712 | Method | `sf_binary_reader_read_f32(sf_binary_reader_t*, float*)` | ✓ aligned | Honors `BigEndian`. |
| `public float[] ReadSingles(int count)` | BinaryReaderEx.cs:723 | Method | `sf_binary_reader_read_f32s(sf_binary_reader_t*, size_t n, float*)` | ✓ aligned | Caller supplies output array. |
| `public float GetSingle(long offset)` | BinaryReaderEx.cs:734 | Method | `sf_binary_reader_get_f32(sf_binary_reader_t*, int64_t off, float*)` | ✓ aligned | Restores original stream cursor. |
| `public float[] GetSingles(long offset, int count)` | BinaryReaderEx.cs:742 | Method | `sf_binary_reader_get_f32s(sf_binary_reader_t*, int64_t off, size_t n, float*)` | ✓ aligned | Restores original stream cursor. |
| `public float AssertSingle(params float[] options)` | BinaryReaderEx.cs:750 | Method | `sf_binary_reader_assert_f32` / `sf_binary_reader_assert_f32_one` | ✓ aligned | Compares exact read float bit/value as upstream `Equals`. |
| `public double ReadDouble()` | BinaryReaderEx.cs:760 | Method | `sf_binary_reader_read_f64(sf_binary_reader_t*, double*)` | ✓ aligned | Honors `BigEndian`. |
| `public double[] ReadDoubles(int count)` | BinaryReaderEx.cs:771 | Method | `sf_binary_reader_read_f64s(sf_binary_reader_t*, size_t n, double*)` | ✓ aligned | Caller supplies output array. |
| `public double GetDouble(long offset)` | BinaryReaderEx.cs:782 | Method | `sf_binary_reader_get_f64(sf_binary_reader_t*, int64_t off, double*)` | ✓ aligned | Restores original stream cursor. |
| `public double[] GetDoubles(long offset, int count)` | BinaryReaderEx.cs:790 | Method | `sf_binary_reader_get_f64s(sf_binary_reader_t*, int64_t off, size_t n, double*)` | ✓ aligned | Restores original stream cursor. |
| `public double AssertDouble(params double[] options)` | BinaryReaderEx.cs:798 | Method | `sf_binary_reader_assert_f64` / `sf_binary_reader_assert_f64_one` | ✓ aligned | Compares exact read double bit/value as upstream `Equals`. |
| `public TEnum ReadEnum8<TEnum>()` | BinaryReaderEx.cs:820 | Generic method | `sf_binary_reader_read_enum_8(sf_binary_reader_t*, size_t n_options, const uint8_t*, uint8_t*)` | ✓ aligned | Generic expanded to width-specific validation; signed C enum bytes pass as two's-complement options. |
| `public TEnum GetEnum8<TEnum>(long position)` | BinaryReaderEx.cs:834 | Generic method | `sf_binary_reader_get_enum_8(sf_binary_reader_t*, int64_t off, size_t n_options, const uint8_t*, uint8_t*)` | ✓ aligned | Restores original stream cursor. |
| `public TEnum ReadEnum16<TEnum>()` | BinaryReaderEx.cs:845 | Generic method | `sf_binary_reader_read_enum_16(sf_binary_reader_t*, size_t n_options, const uint16_t*, uint16_t*)` | ✓ aligned | Generic expanded to width-specific validation. |
| `public TEnum GetEnum16<TEnum>(long position)` | BinaryReaderEx.cs:858 | Generic method | `sf_binary_reader_get_enum_16(sf_binary_reader_t*, int64_t off, size_t n_options, const uint16_t*, uint16_t*)` | ✓ aligned | Restores original stream cursor. |
| `public TEnum ReadEnum32<TEnum>()` | BinaryReaderEx.cs:869 | Generic method | `sf_binary_reader_read_enum_32(sf_binary_reader_t*, size_t n_options, const uint32_t*, uint32_t*)` | ✓ aligned | Generic expanded to width-specific validation. |
| `public TEnum GetEnum32<TEnum>(long position)` | BinaryReaderEx.cs:882 | Generic method | `sf_binary_reader_get_enum_32(sf_binary_reader_t*, int64_t off, size_t n_options, const uint32_t*, uint32_t*)` | ✓ aligned | Restores original stream cursor. |
| `public TEnum ReadEnum64<TEnum>()` | BinaryReaderEx.cs:893 | Generic method | `sf_binary_reader_read_enum_64(sf_binary_reader_t*, size_t n_options, const uint64_t*, uint64_t*)` | ✓ aligned | Generic expanded to width-specific validation. |
| `public TEnum GetEnum64<TEnum>(long position)` | BinaryReaderEx.cs:906 | Generic method | `sf_binary_reader_get_enum_64(sf_binary_reader_t*, int64_t off, size_t n_options, const uint64_t*, uint64_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string ReadASCII()` | BinaryReaderEx.cs:945 | Method | `sf_binary_reader_read_ascii(sf_binary_reader_t*, char**, size_t*)` | ✓ aligned | UTF-8 output; caller frees with `sf_free`. |
| `public string ReadASCII(int length)` | BinaryReaderEx.cs:953 | Method | `sf_binary_reader_read_ascii_n(sf_binary_reader_t*, size_t, char**, size_t*)` | ✓ aligned | Fixed byte length, no terminator required. |
| `public string GetASCII(long offset)` | BinaryReaderEx.cs:961 | Method | `sf_binary_reader_get_ascii(sf_binary_reader_t*, int64_t off, char**, size_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string GetASCII(long offset, int length)` | BinaryReaderEx.cs:972 | Method | `sf_binary_reader_get_ascii_n(sf_binary_reader_t*, int64_t off, size_t, char**, size_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string AssertASCII(params string[] values)` | BinaryReaderEx.cs:983 | Method | `sf_binary_reader_assert_ascii(sf_binary_reader_t*, const char*)` | ~ partial | Existing C API supports single expected string only. |
| `public string ReadShiftJIS()` | BinaryReaderEx.cs:1001 | Method | `sf_binary_reader_read_shift_jis(sf_binary_reader_t*, char**, size_t*)` | ✓ aligned | UTF-8 output; caller frees with `sf_free`. |
| `public string ReadShiftJIS(int length)` | BinaryReaderEx.cs:1009 | Method | `sf_binary_reader_read_shift_jis_n(sf_binary_reader_t*, size_t, char**, size_t*)` | ✓ aligned | Fixed byte length, no terminator required. |
| `public string GetShiftJIS(long offset)` | BinaryReaderEx.cs:1017 | Method | `sf_binary_reader_get_shift_jis(sf_binary_reader_t*, int64_t off, char**, size_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string GetShiftJIS(long offset, int length)` | BinaryReaderEx.cs:1028 | Method | `sf_binary_reader_get_shift_jis_n(sf_binary_reader_t*, int64_t off, size_t, char**, size_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string ReadUTF16()` | BinaryReaderEx.cs:1039 | Method | `sf_binary_reader_read_utf16(sf_binary_reader_t*, char**, size_t*)` | ✓ aligned | Endianness follows reader `BigEndian`. |
| `public string GetUTF16(long offset)` | BinaryReaderEx.cs:1059 | Method | `sf_binary_reader_get_utf16(sf_binary_reader_t*, int64_t off, char**, size_t*)` | ✓ aligned | Restores original stream cursor. |
| `public string ReadFixStr(int size)` | BinaryReaderEx.cs:1070 | Method | `sf_binary_reader_read_fix_str(sf_binary_reader_t*, size_t, char**, size_t*)` | ✓ aligned | Fixed-size Shift-JIS field, truncates at NUL. |
| `public string ReadFixStrW(int size)` | BinaryReaderEx.cs:1085 | Method | `sf_binary_reader_read_fix_str_w(sf_binary_reader_t*, size_t, char**, size_t*)` | ✓ aligned | Fixed-size UTF-16 field, truncates at NUL pair. |
| `public Vector2 ReadVector2()` | BinaryReaderEx.cs:1109 | Method | `sf_binary_reader_read_vec2(sf_binary_reader_t*, sf_vec2_t*)` | ✓ aligned | Reads two f32 values. |
| `public Vector3 ReadVector3()` | BinaryReaderEx.cs:1119 | Method | `sf_binary_reader_read_vec3(sf_binary_reader_t*, sf_vec3_t*)` | ✓ aligned | Reads three f32 values. |
| `public Vector4 ReadVector4()` | BinaryReaderEx.cs:1130 | Method | `sf_binary_reader_read_vec4(sf_binary_reader_t*, sf_vec4_t*)` | ✓ aligned | Reads four f32 values. |
| `public Quaternion ReadQuaternion()` | BinaryReaderEx.cs:1142 | Method | `sf_binary_reader_read_quat(sf_binary_reader_t*, sf_quat_t*)` | ✓ aligned | Reads XYZW f32 order. |
| `public Vector3 Read11_11_10Vector3()` | BinaryReaderEx.cs:1154 | Method | `sf_binary_reader_read_11_11_10_vec3(sf_binary_reader_t*, sf_vec3_t*)` | ✓ aligned | Clean C API rename; old `read_vec3_11_11_10` removed. |
| `public void AssertPattern(int length, byte pattern)` | BinaryReaderEx.cs:1166 | Method | `sf_binary_reader_assert_pattern(sf_binary_reader_t*, size_t, uint8_t)` | ✓ aligned | Returns `SF_ERR_BAD_MAGIC` on mismatch. |
| `public Color ReadARGB()` | BinaryReaderEx.cs:1179 | Method | `sf_binary_reader_read_argb(sf_binary_reader_t*, sf_color_t*)` | ✓ aligned | Color returned as RGBA fields. |
| `public Color ReadABGR()` | BinaryReaderEx.cs:1191 | Method | `sf_binary_reader_read_abgr(sf_binary_reader_t*, sf_color_t*)` | ✓ aligned | Color returned as RGBA fields. |
| `public Color ReadRGBA()` | BinaryReaderEx.cs:1203 | Method | `sf_binary_reader_read_rgba(sf_binary_reader_t*, sf_color_t*)` | ✓ aligned | Color returned as RGBA fields. |
| `public Color ReadBGRA()` | BinaryReaderEx.cs:1215 | Method | `sf_binary_reader_read_bgra(sf_binary_reader_t*, sf_color_t*)` | ✓ aligned | Color returned as RGBA fields. |
| `public void Dispose()` | BinaryReaderEx.cs:1247 | Method | `sf_binary_reader_destroy(sf_binary_reader_t*)` | ✓ aligned | Reader frees only its own allocations and leaves borrowed stream open. |
