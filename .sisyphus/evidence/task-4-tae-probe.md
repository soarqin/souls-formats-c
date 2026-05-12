# Task 4: ER c0000.anibnd.dcx TAEFormat probe

Probe target: `/chr/c0000.anibnd.dcx` from Elden Ring `Data0.bhd`..`Data3.bhd`.

## Extraction result

- Found archive: `Data3`
- Found BHD5 path: `/chr/c0000.anibnd.dcx`
- `sf_path_hash_64`: `0x00000000F8630FB1`
- Data3 folded 64-bit fallback hash: `0xB5D79FD786383451`
- Decompressed BND4 size: `67,960,282` bytes
- c0000.anibnd BND4 entry count: `654`
- `.tae` entry count: `639`
- Sample count: `5`

## Sample TAE headers

| # | Sample path | Version | Format | Format byte | AnimCount | AnimOffset | AnimGroupsOffset | Flags |
|---|-------------|---------|--------|-------------|-----------|------------|------------------|-------|
| 1 | `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a00.tae` | `0x1000D` | `SDT` | `0xFF` | `1839` | `0x110` | `0x7400` | `0100010202010101` |
| 2 | `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a02.tae` | `0x1000D` | `SDT` | `0xFF` | `427` | `0x110` | `0x1BC0` | `0100010202010101` |
| 3 | `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a03.tae` | `0x1000D` | `SDT` | `0xFF` | `419` | `0x110` | `0x1B40` | `0100010202010101` |
| 4 | `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a10.tae` | `0x1000D` | `SDT` | `0xFF` | `240` | `0x110` | `0x1010` | `0100010202010101` |
| 5 | `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a12.tae` | `0x1000D` | `SDT` | `0xFF` | `239` | `0x110` | `0x1000` | `0100010202010101` |

All five sampled headers are little-endian (`BigEndianByte=0`) and 64-bit (`Is64Byte=0xFF`).
For upstream `TAE.cs`, version `0x1000D` maps to `TAEFormat.SDT` for Sekiro/Elden Ring.

## Minimal hex dump

Smallest `.tae` sample: `N:\GR\data\INTERROOT_win64\chr\c0000\tae\a107.tae` (`288` bytes).

First 64 bytes:

```text
0000: 54 41 45 20 00 00 00 FF 0D 00 01 00 20 01 00 00
0010: 40 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
0020: 50 00 00 00 00 00 00 00 80 00 00 00 00 00 00 00
0030: 25 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## ALERT

None. `VERSION_MISMATCH_COUNT=0`; all sampled TAE versions are `0x1000D`.

Raw probe output: `.sisyphus/evidence/task-4-tae-probe.txt`.
