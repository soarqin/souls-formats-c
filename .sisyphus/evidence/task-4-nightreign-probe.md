# Nightreign MSB Header Probe — Task 4

## Status: BLOCKED — BHD5 open returns SF_ERR_OUT_OF_RANGE

### Root Cause
`sf_bhd5_open()` returns `SF_ERR_OUT_OF_RANGE` for all games (ER, NR, Sekiro, AC6).
This is a pre-existing bug: the RSA-decrypted BHD5 content has variable-length chunks
(leading zeros stripped by `sfi_rsa_decrypt_pkcs1`), causing `sf_istream_seek` to fail
when `buckets_offset` exceeds the decrypted buffer size.

The e2e tests have always been IGNORING in this environment (Unity IGNORE = PASS).
The RSA test `test_rsa_real_eldenring_data0` passes (first 256 bytes decrypt to "BHD5"),
but the full file decryption produces incorrect offsets.

### ER MSB header hex dump
- Path: `EXTRACTION FAILED — sf_bhd5_open returned SF_ERR_OUT_OF_RANGE`
- Cannot extract without working BHD5 open

### NR MSB header hex dump
- Path: `EXTRACTION FAILED — sf_bhd5_open returned SF_ERR_OUT_OF_RANGE`
- Cannot extract without working BHD5 open

## Diff + decision

**VERDICT: UNKNOWN** — probe could not run due to BHD5 open bug.

Community evidence: The community reports MSBE (ER's MSB variant) can read Nightreign MSB
files with the same field layout. No upstream official confirmation exists.

**Working assumption: A (compatible)** — proceed with MSBE implementation for both ER and NR.
Verification will happen during T38 (MSBE e2e via Nightreign).

## Action Items
1. Fix BHD5 open bug (SF_ERR_OUT_OF_RANGE from variable-length RSA chunk decryption)
   — this is a pre-existing issue blocking ALL e2e tests
2. Re-run probe after BHD5 fix to get actual verdict
3. If T38 (NR e2e) fails, revisit MSBE compatibility

## BHD5 Bug Details
- `sfi_rsa_decrypt_pkcs1` strips leading zeros from each 256-byte chunk
- This produces variable-length output per chunk
- `rsa_unwrap_bhd5` concatenates chunks → total size < original file size
- `parse_bhd5` reads `buckets_offset` from header → seeks to that offset
- `sf_istream_seek` fails: `pos > mem_size` → `SF_ERR_OUT_OF_RANGE`
- Fix: preserve fixed chunk size (don't strip leading zeros, or pad to fixed size)
