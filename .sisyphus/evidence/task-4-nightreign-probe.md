# Nightreign MSB Header Probe

## ER MSB header hex dump

- Path: `ER BHD5 extraction failed before MSB header read`
- Decompressed size: 0 bytes
- Magic: `FAIL`
- Version field: -1
- Entry list count field: -1

```text
0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## NR MSB header hex dump

- Path: `BHD5 open failed before MSB extraction`
- Decompressed size: 0 bytes
- Magic: `FAIL`
- Version field: -1
- Entry list count field: -1

```text
0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## Diff + decision

- Magic bytes: match (`FAIL` vs `FAIL`)
- Version field: match (-1 vs -1)
- Entry list count field: match (-1 vs -1)
- First 64 bytes: identical

VERDICT: DIVERGED (C)
