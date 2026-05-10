
## F3 QA Finding (2026-05-11) — BHD5 open of ER Data0 returns SF_ERR_OUT_OF_RANGE

E2E tests skip with "ER copy or BHD5 not available" even when the ER copy
AND Oodle DLLs are present at the configured paths:

- `C:/Games/ELDEN RING/Game/Data0.bhd` — accessible via WSL interop, GetFileAttributesW returns 0x20.
- Oodle DLL at both `C:/Games/ELDEN RING/Game/oo2core_6_win64.dll` and `~/dev/oodle/oo2core_*_win64.dll`.

Direct probe of `sf_bhd5_open()` against ER Data0.bhd/Data0.bdt returns
`SF_ERR_OUT_OF_RANGE` (code 7) — meaning the BHD5 parser hits a bounds
issue before BHD5 is even returned. The encrypted ER BHD5 needs RSA
decryption first (the on-disk file starts with RSA-encrypted blocks).

This is acceptable per the QA spec (skip-as-pass), but it means the
**REAL** end-to-end ER chain has never actually been exercised end-to-end
on a real game install. The Phase 3 `er_extract_from_data0` helper is
unverified against real data. Recommend Phase 3 owner reproduce this
locally and either (a) wire RSA pre-decrypt of the BHD into
`sf_bhd5_open` for ENCRYPTED games, or (b) document that callers must
pre-decrypt and feed a decrypted .bhd buffer.
