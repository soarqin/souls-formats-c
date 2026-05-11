# Upstream Baseline: SoulsFormatsNEXT

## Pinned Commit

| Field       | Value                                                                    |
|-------------|--------------------------------------------------------------------------|
| SHA         | 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a                                |
| Date        | 2026-02-06 01:33:14 -0500                                                |
| Subject     | Merge remote-tracking branch 'origin/master'                             |
| Branch      | master                                                                   |
| `.cs` files | 413                                                                      |
| Total LOC   | 151635                                                                   |

## Source Files

| File | Line Count | Classes Defined |
|------|------------|-----------------|
| `SoulsFormats/Formats/ESD.cs` | 875 | `ESD`, `State`, `Condition`, `CommandCall` |

## Game Data Snapshots

| Game | Install Path | Index File | sha256 (first 12 chars) | Snapped |
|------|-------------|------------|------------------------|---------|
| Elden Ring | `/mnt/c/Games/ELDEN RING/Game` | `Data0.bhd` | `fbe82e31c36b` | 2026-05-11 |
| Sekiro | `/mnt/c/Games/Sekiro` | `Data1.bhd` | `a0058f49a04e` | 2026-05-11 |
| Nightreign | `/mnt/c/Games/ELDEN RING NIGHTREIGN/Game` | `data0.bhd` | `de0d6bc893b9` | 2026-05-11 |
| AC6 | `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/Game` | `Data0.bhd` | `35c8fd511e78` | 2026-05-12 |

```text
Elden Ring: fbe82e31c36b7a58258a9d318d0a20d8ae626beda952811106c2b0029194981a
Sekiro: a0058f49a04e98fde61cf23f96b155b809b9c820b007c0fb181452daffb5928d
Nightreign: de0d6bc893b98f515f5cd1f4e9b17787dd903ec865b025dd177a82799552516b
AC6: 35c8fd511e78caf6998a553a54b30649af2e1c24ea4594ba5d027161d751ac05
```

Risk note: If the user updates any game, MSB/ESD field layouts may change; e2e fixtures only guaranteed for this snapshot.

### Phase 6 e2e File Snapshots (Elden Ring)

| File | BHD5 Path | Archive | sha256 (archive, first 12 chars) | Snapped |
|------|-----------|---------|----------------------------------|---------|
| `c0000.chrbnd.dcx` | `/chr/c0000.chrbnd.dcx` | `Data1.bhd` | `fd7cccc12d2f` | 2026-05-12 |
| `allmaterial.matbinbnd.dcx` | `/material/allmaterial.matbinbnd.dcx` | `Data0.bhd` | `fbe82e31c36b` | 2026-05-12 |

```text
ER Data0.bhd: fbe82e31c36b7a58258a9d318d0a20d8ae626beda952811106c2b0029194981a
ER Data1.bhd: fd7cccc12d2fe57989a2627fdc02a2eb29bfd8837d2e9d7aa05f8fe25170c714
ER Data2.bhd: c2e0bed43bfa6ee5c8397baee8c49f9ac4245c43fdcf0ba36aa04460e2ae13a0
ER Data3.bhd: 6094382a24bf7c1bb3e07697a31f1fc9d74840f2aec7deecaa6431a92f81d1e5
```

Note: sha256 values above are of the BHD5 archive index files (not the extracted DCX content).
`c0000.chrbnd.dcx` is in Data1 (not Data0); `allmaterial.matbinbnd.dcx` is in Data0.
Phase 6 e2e tests will verify archive sha256 as a sanity check; if the game is patched and
sha256 mismatches, tests SKIP with a log message rather than failing (avoids false failures).

## Re-audit policy

When upstream advances past 50 commits on the recorded branch, re-survey
BinaryReaderEx, BinaryWriterEx, DCX.cs, RegulationDecryptor.cs,
SL2Decryptor.cs, HashHelper.cs, Oodle.cs only. Other files re-audited on
next phase mapping.
