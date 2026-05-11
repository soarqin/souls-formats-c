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

## Re-audit policy

When upstream advances past 50 commits on the recorded branch, re-survey
BinaryReaderEx, BinaryWriterEx, DCX.cs, RegulationDecryptor.cs,
SL2Decryptor.cs, HashHelper.cs, Oodle.cs only. Other files re-audited on
next phase mapping.
