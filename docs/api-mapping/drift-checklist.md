# Drift Checklist

All Phase 0/1/2 drift items; ticked as fixes land in Wave 2/3 tasks.

## Phase 1 / BinaryReaderEx

- [ ] [BinaryReaderEx] plural reads (read_i8s/u8s/i16s/u16s/i32s/u32s/i64s/u64s/f32s/f64s/bools/varints) (closes when Task 8 ships)
- [ ] [BinaryReaderEx] Get* coverage (get_bool/i8/u8/u16/i16/f32/f64/varint) (closes when Task 8 ships)
- [ ] [BinaryReaderEx] Get* plural variants (get_bools/i8s/u8s/u16s/i16s/f32s/f64s/varints) (closes when Task 8 ships)
- [ ] [BinaryReaderEx] GetASCII / GetUTF16 / GetShiftJIS (with and without length) (closes when Task 8 ships)
- [ ] [BinaryReaderEx] assert_* multi-option signature (closes when Task 8 ships)
- [ ] [BinaryReaderEx] assert_i8 / sbyte / i16 / boolean / single / double / i64 / varint missing variants (closes when Task 8 ships)
- [ ] [BinaryReaderEx] read_enum_8 / 16 / 32 / 64 (closes when Task 8 ships)
- [ ] [BinaryReaderEx] is_flexible per-reader flag + global default setter (closes when Task 8 ships)
- [ ] [BinaryReaderEx] sf_binary_reader_stream() accessor (closes when Task 8 ships)
- [ ] [BinaryReaderEx] rename read_vec3_11_11_10 → read_11_11_10_vec3 (closes when Task 8 ships)

## Phase 1 / BinaryWriterEx

- [ ] [BinaryWriterEx] Reserve/Fill missing 7 types (bool / i8 / u8 / i16 / u16 / f32 / f64) (closes when Task 9 ships)
- [ ] [BinaryWriterEx] write_*s plural writes (12 types) (closes when Task 9 ships)
- [ ] [BinaryWriterEx] pad_FF shorthand (closes when Task 9 ships)
- [ ] [BinaryWriterEx] rename pattern → write_pattern (closes when Task 9 ships)
- [ ] [BinaryWriterEx] to_array() and finish_bytes() 3-mode finish (closes when Task 9 ships)
- [ ] [BinaryWriterEx] sf_binary_writer_stream() accessor (closes when Task 9 ships)

## Phase 1 / PathHelper

- [ ] [PathHelper] sf_path_backup missing entirely (closes when Task 10 ships)
- [ ] [PathHelper] sf_path_get_real_extension missing entirely (closes when Task 10 ships)
- [ ] [PathHelper] sf_path_get_real_file_name missing entirely (closes when Task 10 ships)

## Phase 1 / HashHelper

- [x] [HashHelper] sf_is_prime missing (closed by Task 12)

## Phase 1 / SFUtil

- [ ] [SFUtil] sf_get_decompressed_reader missing (closes when Task 12 ships)

## Phase 1 / Math

- [ ] [Math] _Static_assert size guards missing for sf_vec2/3/4, sf_quat, sf_mat4, sf_color (closes when Task 13 ships)

## Phase 2 / DCX

- [ ] [DCX] flat sf_dcx_params_t must become tagged union sf_dcx_compression_info_t with 9 variants (closes when Task 14 ships)
- [ ] [DCX] preset enums missing (sf_dcx_default_type_t, sf_dcx_dflt_compression_preset_t, sf_dcx_krak_compression_preset_t) (closes when Task 14 ships)
- [ ] [DCX] factory helpers missing (closes when Task 14 ships)
- [ ] [DCX] sf_dcx_is_* family missing (closes when Task 14 ships)
- [ ] [DCX] path/stream overloads missing (closes when Task 14 ships)

## Phase 2 / Oodle

- [ ] [Oodle] sf_oodle_lz_compressor_t not public (closes when Task 15 ships)
- [ ] [Oodle] sf_oodle_lz_compression_level_t not public (closes when Task 15 ships)
- [ ] [Oodle] all other OodleLZ_* enums not public (closes when Task 15 ships)
- [ ] [Oodle] sf_oodle_version() returns int instead of enum (closes when Task 15 ships)

## Phase 2 / RegulationDecryptor

- [ ] [RegulationDecryptor] sf_regulation.h missing (no public API) (closes when Task 16 ships)
- [ ] [RegulationDecryptor] game-specific convenience wrappers missing (closes when Task 16 ships)

## Phase 2 / SL2Decryptor

- [ ] [SL2Decryptor] sf_sl2.h missing (no public API) (closes when Task 17 ships)
- [ ] [SL2Decryptor] key getter functions missing (closes when Task 17 ships)
