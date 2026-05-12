# T2.2 Magic-check helper macro audit

Date: 2026-05-12

Required count check:

```text
$ grep -rEn 'SF_ERR_BAD_MAGIC' src/ | wc -l
123
```

## Verdict: SKIP

Only **7/20** sampled sites matched the narrow helper pattern of
`sf_binary_reader_assert_ascii(...)` followed by immediate `return r` on failure.
That is **35%**, below the required 80% (16/20) extraction threshold.

Do not add `SF_ASSERT_MAGIC` yet: most sampled `SF_ERR_BAD_MAGIC` checks validate
format-specific structural invariants, embedded-NUL byte magics, dual magics, or
cleanup/goto flows that a return-on-fail ASCII macro would not model safely.

## Sample table

| # | Site | Pattern | Uses `assert_ascii` / `assert_u32`? | Uniform with proposed return-on-fail helper? |
|---:|---|---|---|---|
| 1 | `src/archive/bnd3.c:225` | `assert_ascii("BND3"); if (r != SF_OK) return r;` | `assert_ascii` | Yes |
| 2 | `src/archive/bnd4.c:189` | `assert_ascii("BND4"); if (r != SF_OK) return r;` | `assert_ascii` | Yes |
| 3 | `src/archive/bnd4.c:237` | nonzero hash-table offset rejected after conditional branch | No | No |
| 4 | `src/archive/bxf3.c:213` | `assert_ascii("BDF3")` then immediate return on fail | `assert_ascii` | Yes |
| 5 | `src/archive/bxf3.c:248` | `assert_ascii("BHF3"); if (r != SF_OK) return r;` | `assert_ascii` | Yes |
| 6 | `src/archive/bxf4.c:222` | `assert_ascii("BDF4"); if (r != SF_OK) return r;` | `assert_ascii` | Yes |
| 7 | `src/archive/bxf4.c:270` | `assert_ascii("BHF4"); if (r != SF_OK) return r;` | `assert_ascii` | Yes |
| 8 | `src/archive/bxf4.c:307` | computed file-header size mismatch goes through cleanup path | No | No |
| 9 | `src/archive/bhd5.c:193` | decrypted buffer checked with `has_bhd5_magic()` before reader exists | No | No |
| 10 | `src/archive/bhd5.c:333` | `assert_ascii("BHD5")` but failure uses `goto out` cleanup path | `assert_ascii` | No |
| 11 | `src/archive/bhd5.c:341` | endian sentinel must be `0` or `-1` | No | No |
| 12 | `src/archive/tpf.c:400` | reads 4 bytes and compares embedded-NUL `"TPF\\0"` | No | No |
| 13 | `src/text/fmg.c:139` | header byte invariant via `assert_u8_one(0)`, no ASCII magic | Neither | No |
| 14 | `src/param/param.c:266` | endian byte must be `0` or `0xFF` | No | No |
| 15 | `src/map/msbe/msbe.c:61` | list-name string must match expected section name | No | No |
| 16 | `src/geom/flver2.c:50` | reads 6 bytes and compares embedded-NUL `"FLVER\\0"` | No | No |
| 17 | `src/script/emevd.c:53` | reads 4 bytes and compares embedded-NUL `EVD\\0` byte array | No | No |
| 18 | `src/script/esd.c:502` | accepts either `"fSSL"` or `"fsSL"` and sets format mode | No | No |
| 19 | `src/effects/tae.c:125` | `assert_ascii("TAE ")` then immediate return on fail | `assert_ascii` | Yes |
| 20 | `src/effects/fxr3.c:548` | raw buffer `rd_need()` + embedded-NUL `"FXR\\0"` + `goto fail` | No | No |

## Divergence summary

- **Embedded-NUL binary magic**: TPF, FLVER2, EMEVD, FXR3 cannot use the current
  public `sf_binary_reader_assert_ascii()` because it derives length via
  `strlen()`.
- **Cleanup paths**: BHD5 and several archive checks use `goto out/fail`; a macro
  with `return _r` would skip cleanup.
- **Semantic invariants**: PARAM, FMG, MSBE, BND/BXF header-size checks, and ESD
  mode detection report `SF_ERR_BAD_MAGIC` for upstream structural validation, not
  a single ASCII magic comparison.

## LOC delta

No source LOC delta. Extraction was skipped.
