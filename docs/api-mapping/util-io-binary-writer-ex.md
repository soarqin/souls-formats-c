# BinaryWriterEx API Mapping

Upstream reference: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryWriterEx.cs` at pinned commit documented in `UPSTREAM.md`.

| Upstream signature | Upstream loc (BinaryWriterEx.cs:LINE) | Kind | Our API (or 未实现) | Status | Notes |
|---|---:|---|---|---|---|
| `public class BinaryWriterEx : IDisposable` | 13 | type | `sf_binary_writer_t` | ✓ aligned | Opaque C handle. |
| `public bool IsDisposed { get; private set; }` | 22 | property | `sf_binary_writer_finish`, `sf_binary_writer_finish_bytes`, `sf_binary_writer_destroy` | ~ partial | C tracks closed state internally; no public getter. |
| `public bool BigEndian { get; set; }` | 27 | property | `sf_binary_writer_big_endian`, `sf_binary_writer_set_big_endian` | ✓ aligned | Mutable per-writer flag. |
| `public bool VarintLong { get; set; }` | 32 | property | `sf_binary_writer_varint_long`, `sf_binary_writer_set_varint_long` | ✓ aligned | Mutable per-writer flag. |
| `public int VarintSize => VarintLong ? 8 : 4` | 37 | property | 未实现 | _skipped_ | Derivable from `sf_binary_writer_varint_long`. |
| `public Stream Stream { get; }` | 42 | property | `sf_binary_writer_stream` | ✓ aligned | Borrowed `sf_ostream_t*`; caller must not close through writer. |
| `public long Position { get; set; }` | 47 | property | `sf_binary_writer_position`, `sf_binary_writer_step_in`, `sf_binary_writer_step_out`, `sf_ostream_seek` | ~ partial | Direct setter remains on stream API. |
| `public long Length => Stream.Length` | 56 | property | `sf_binary_writer_length` | ✓ aligned | Returns stream length. |
| `BinaryWriterEx(bool bigEndian, string path)` | 61 | constructor | `sf_ostream_open_file` + `sf_binary_writer_create` | ✓ aligned | C separates stream construction from writer construction. |
| `BinaryWriterEx(bool bigEndian)` | 66 | constructor | `sf_ostream_open_memory` + `sf_binary_writer_create` | ✓ aligned | C separates memory stream construction from writer construction. |
| `BinaryWriterEx(bool bigEndian, Stream stream, bool leaveOpen = false)` | 71 | constructor | `sf_binary_writer_create` | ~ partial | Borrowed-stream C model behaves like `leaveOpen=true`; caller closes stream. |
| `private void WriteReversedBytes(byte[] bytes)` | 80 | helper | internal byte-swap helpers | _skipped_ | Internal implementation detail. |
| `private void Reserve(string name, string typeName, int length)` | 86 | helper | internal reservation list | ✓ aligned | C keys by `(name, type tag)`; reserves in-band `0xFE` bytes. |
| `private long Fill(string name, string typeName)` | 97 | helper | internal reservation pop | ✓ aligned | C removes reservation before backfill. |
| `public void Finish()` | 110 | method | `sf_binary_writer_finish` | ✓ aligned | Verifies no pending reservations and closes writer handle. |
| `public byte[] ToArray()` | 122 | method | `sf_binary_writer_to_array` | ✓ aligned | Snapshots bytes without closing writer. |
| `public byte[] FinishBytes()` | 142 | method | `sf_binary_writer_finish_bytes` | ✓ aligned | Snapshot + verify + close writer handle. |
| `public void StepIn(long offset)` | 153 | method | `sf_binary_writer_step_in` | ✓ aligned | Pushes current position and seeks. |
| `public void StepOut()` | 162 | method | `sf_binary_writer_step_out` | ✓ aligned | Pops prior position; empty stack maps to error. |
| `public void Pad(int align, byte custom)` | 173 | method | `sf_binary_writer_pad_byte` | ✓ aligned | Explicit custom byte. |
| `public void Pad(int align)` | 182 | method | `sf_binary_writer_pad` | ✓ aligned | Pads with `0x00`. |
| `public void PadFF(int align)` | 190 | method | `sf_binary_writer_pad_ff` | ✓ aligned | Pads with `0xFF`. |
| `public void PadRelative(long start, int align)` | 198 | method | `sf_binary_writer_pad_relative` | ✓ aligned | Pads with `0x00` relative to start. |
| `public void WriteBoolean(bool value)` | 208 | method | `sf_binary_writer_write_bool` | ✓ aligned | One-byte boolean. |
| `public void WriteBooleans(IList<bool> values)` | 216 | method | `sf_binary_writer_write_bools` | ✓ aligned | C uses `size_t count, const bool *values`. |
| `public void ReserveBoolean(string name)` | 225 | method | `sf_binary_writer_reserve_bool` | ✓ aligned | Type tag `bool`. |
| `public void FillBoolean(string name, bool value)` | 233 | method | `sf_binary_writer_fill_bool` | ✓ aligned | Requires matching `bool` reservation. |
| `public void WriteSByte(sbyte value)` | 245 | method | `sf_binary_writer_write_i8` | ✓ aligned | Signed 8-bit. |
| `public void WriteSBytes(IList<sbyte> values)` | 253 | method | `sf_binary_writer_write_i8s` | ✓ aligned | C array form. |
| `public void ReserveSByte(string name)` | 262 | method | `sf_binary_writer_reserve_i8` | ✓ aligned | Type tag `i8`. |
| `public void FillSByte(string name, sbyte value)` | 270 | method | `sf_binary_writer_fill_i8` | ✓ aligned | Requires matching `i8` reservation. |
| `public void WriteByte(byte value)` | 282 | method | `sf_binary_writer_write_u8` | ✓ aligned | Unsigned 8-bit. |
| `public void WriteBytes(byte[] bytes)` | 290 | method | `sf_binary_writer_write_bytes` | ✓ aligned | Raw byte slice. |
| `public void WriteBytes(IList<byte> values)` | 298 | method | `sf_binary_writer_write_u8s` | ✓ aligned | C array form. |
| `public void ReserveByte(string name)` | 307 | method | `sf_binary_writer_reserve_u8` | ✓ aligned | Type tag `u8`. |
| `public void FillByte(string name, byte value)` | 315 | method | `sf_binary_writer_fill_u8` | ✓ aligned | Requires matching `u8` reservation. |
| `public void WriteInt16(short value)` | 327 | method | `sf_binary_writer_write_i16` | ✓ aligned | Endian-aware. |
| `public void WriteInt16s(IList<short> values)` | 338 | method | `sf_binary_writer_write_i16s` | ✓ aligned | C array form. |
| `public void ReserveInt16(string name)` | 347 | method | `sf_binary_writer_reserve_i16` | ✓ aligned | Type tag `i16`. |
| `public void FillInt16(string name, short value)` | 355 | method | `sf_binary_writer_fill_i16` | ✓ aligned | Requires matching `i16` reservation. |
| `public void WriteUInt16(ushort value)` | 367 | method | `sf_binary_writer_write_u16` | ✓ aligned | Endian-aware. |
| `public void WriteUInt16s(IList<ushort> values)` | 378 | method | `sf_binary_writer_write_u16s` | ✓ aligned | C array form. |
| `public void ReserveUInt16(string name)` | 387 | method | `sf_binary_writer_reserve_u16` | ✓ aligned | Type tag `u16`. |
| `public void FillUInt16(string name, ushort value)` | 395 | method | `sf_binary_writer_fill_u16` | ✓ aligned | Requires matching `u16` reservation. |
| `public void WriteInt32(int value)` | 407 | method | `sf_binary_writer_write_i32` | ✓ aligned | Endian-aware. |
| `public void WriteInt32s(IList<int> values)` | 418 | method | `sf_binary_writer_write_i32s` | ✓ aligned | C array form. |
| `public void ReserveInt32(string name)` | 427 | method | `sf_binary_writer_reserve_i32` | ✓ aligned | Type tag `i32`. |
| `public void FillInt32(string name, int value)` | 435 | method | `sf_binary_writer_fill_i32` | ✓ aligned | Requires matching `i32` reservation. |
| `public void WriteUInt32(uint value)` | 447 | method | `sf_binary_writer_write_u32` | ✓ aligned | Endian-aware. |
| `public void WriteUInt32s(IList<uint> values)` | 458 | method | `sf_binary_writer_write_u32s` | ✓ aligned | C array form. |
| `public void ReserveUInt32(string name)` | 467 | method | `sf_binary_writer_reserve_u32` | ✓ aligned | Type tag `u32`. |
| `public void FillUInt32(string name, uint value)` | 475 | method | `sf_binary_writer_fill_u32` | ✓ aligned | Requires matching `u32` reservation. |
| `public void WriteInt64(long value)` | 487 | method | `sf_binary_writer_write_i64` | ✓ aligned | Endian-aware. |
| `public void WriteInt64s(IList<long> values)` | 498 | method | `sf_binary_writer_write_i64s` | ✓ aligned | C array form. |
| `public void ReserveInt64(string name)` | 507 | method | `sf_binary_writer_reserve_i64` | ✓ aligned | Type tag `i64`. |
| `public void FillInt64(string name, long value)` | 515 | method | `sf_binary_writer_fill_i64` | ✓ aligned | Requires matching `i64` reservation. |
| `public void WriteUInt64(ulong value)` | 527 | method | `sf_binary_writer_write_u64` | ✓ aligned | Endian-aware. |
| `public void WriteUInt64s(IList<ulong> values)` | 538 | method | `sf_binary_writer_write_u64s` | ✓ aligned | C array form. |
| `public void ReserveUInt64(string name)` | 547 | method | `sf_binary_writer_reserve_u64` | ✓ aligned | Type tag `u64`. |
| `public void FillUInt64(string name, ulong value)` | 555 | method | `sf_binary_writer_fill_u64` | ✓ aligned | Requires matching `u64` reservation. |
| `public void WriteVarint(long value)` | 567 | method | `sf_binary_writer_write_varint` | ✓ aligned | Uses current varint-long flag. |
| `public void WriteVarints(IList<long> values)` | 578 | method | `sf_binary_writer_write_varints` | ✓ aligned | Uses current varint-long flag for every element. |
| `public void ReserveVarint(string name)` | 592 | method | `sf_binary_writer_reserve_varint` | ✓ aligned | Type tag `varint32` or `varint64`. |
| `public void FillVarint(string name, long value)` | 603 | method | `sf_binary_writer_fill_varint` | ✓ aligned | Requires matching varint-size reservation. |
| `public void WriteSingle(float value)` | 624 | method | `sf_binary_writer_write_f32` | ✓ aligned | Endian-aware IEEE-754 bits. |
| `public void WriteSingles(IList<float> values)` | 635 | method | `sf_binary_writer_write_f32s` | ✓ aligned | C array form. |
| `public void ReserveSingle(string name)` | 644 | method | `sf_binary_writer_reserve_f32` | ✓ aligned | Type tag `f32`. |
| `public void FillSingle(string name, float value)` | 652 | method | `sf_binary_writer_fill_f32` | ✓ aligned | Requires matching `f32` reservation. |
| `public void WriteDouble(double value)` | 664 | method | `sf_binary_writer_write_f64` | ✓ aligned | Endian-aware IEEE-754 bits. |
| `public void WriteDoubles(IList<double> values)` | 675 | method | `sf_binary_writer_write_f64s` | ✓ aligned | C array form. |
| `public void ReserveDouble(string name)` | 684 | method | `sf_binary_writer_reserve_f64` | ✓ aligned | Type tag `f64`. |
| `public void FillDouble(string name, double value)` | 692 | method | `sf_binary_writer_fill_f64` | ✓ aligned | Requires matching `f64` reservation. |
| `private void WriteChars(string text, Encoding encoding, bool terminate)` | 701 | helper | internal encoding helpers | _skipped_ | Internal implementation detail. |
| `public void WriteASCII(string text, bool terminate = false)` | 712 | method | `sf_binary_writer_write_ascii` | ✓ aligned | UTF-8 boundary; explicit `terminate`. |
| `public void WriteShiftJIS(string text, bool terminate = false)` | 720 | method | `sf_binary_writer_write_shift_jis` | ✓ aligned | UTF-8 boundary; explicit `terminate`. |
| `public void WriteUTF16(string text, bool terminate = false)` | 728 | method | `sf_binary_writer_write_utf16` | ✓ aligned | Big-endian flag selects UTF-16BE/LE. |
| `public void WriteFixStr(string text, int size, byte padding = 0)` | 739 | method | `sf_binary_writer_write_fix_str` | ✓ aligned | UTF-8 boundary; explicit padding. |
| `public void WriteFixStrW(string text, int size, byte padding = 0)` | 753 | method | `sf_binary_writer_write_fix_str_w` | ✓ aligned | Big-endian flag selects UTF-16BE/LE. |
| `public void WriteVector2(Vector2 vector)` | 773 | method | `sf_binary_writer_write_vec2` | ✓ aligned | POD `sf_vec2_t`. |
| `public void WriteVector3(Vector3 vector)` | 782 | method | `sf_binary_writer_write_vec3` | ✓ aligned | POD `sf_vec3_t`. |
| `public void WriteVector4(Vector4 vector)` | 792 | method | `sf_binary_writer_write_vec4` | ✓ aligned | POD `sf_vec4_t`. |
| `public void WritePattern(int length, byte pattern)` | 803 | method | `sf_binary_writer_write_pattern` | ✓ aligned | Renamed from former `sf_binary_writer_pattern`; breaking change. |
| `public void WriteARGB(Color color)` | 817 | method | `sf_binary_writer_write_argb` | ✓ aligned | `sf_color_t` byte fields. |
| `public void WriteABGR(Color color)` | 828 | method | `sf_binary_writer_write_abgr` | ✓ aligned | `sf_color_t` byte fields. |
| `public void WriteRGBA(Color color)` | 839 | method | `sf_binary_writer_write_rgba` | ✓ aligned | `sf_color_t` byte fields. |
| `public void WriteBGRA(Color color)` | 850 | method | `sf_binary_writer_write_bgra` | ✓ aligned | `sf_color_t` byte fields. |
| `protected virtual void Dispose(bool disposing)` | 865 | method | `sf_binary_writer_destroy` | ~ partial | Public C destroy frees without verification; callers use finish first. |
| `public void Dispose()` | 888 | method | `sf_binary_writer_destroy` | ~ partial | C separates verification (`finish`) from free (`destroy`). |
