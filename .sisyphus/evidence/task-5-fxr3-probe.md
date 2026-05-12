# Task 5 — FXR3 empirical probe (`sfxbnd_commoneffects.ffxbnd.dcx`)

Command:

```bash
cmake --build build-mingw --target probe_fxr3_format
./build-mingw/tests/probes/probe_fxr3_format.exe > .sisyphus/evidence/task-5-fxr3-probe.txt 2>&1
```

## Archive

- BHD shard: `FOUND_IN_DATA: 0`
- BND entry count: `14959`
- `.fxr` entry count: `7138`

## 10 sampled `.fxr` headers

| # | Path | Version | Id | Section counts |
|---:|---|---:|---:|---|
| 1 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000001.fxr` | 5 | 1 | statemap=1 state=1 transition=1 container=4 effect=3 action=52 property=114 modifier=0 condprop=0 unkfieldlist=3 field=372 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 2 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000002.fxr` | 5 | 2 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3450 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 3 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000003.fxr` | 5 | 3 | statemap=1 state=2 transition=2 container=28 effect=42 action=647 property=2245 modifier=162 condprop=0 unkfieldlist=27 field=5236 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 4 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000004.fxr` | 5 | 4 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3450 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 5 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000005.fxr` | 5 | 5 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3450 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 6 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000007.fxr` | 5 | 7 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3440 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 7 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000008.fxr` | 5 | 8 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3444 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 8 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000009.fxr` | 5 | 9 | statemap=1 state=2 transition=2 container=28 effect=34 action=527 property=1657 modifier=110 condprop=0 unkfieldlist=27 field=3902 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 9 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000010.fxr` | 5 | 10 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1501 modifier=92 condprop=0 unkfieldlist=27 field=3478 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |
| 10 | `N:\GR\data\INTERROOT_win64\sfx\effect\f000000011.fxr` | 5 | 11 | statemap=1 state=2 transition=2 container=28 effect=32 action=497 property=1489 modifier=92 condprop=0 unkfieldlist=27 field=3450 reference=0 external_value=1 unk_blood_enabler=0 section15=0 |

## Version histogram

- `FXR_VERSION_HIST: DS3=0 Sekiro=10`
- `UNKNOWN_VERSION`: none observed.

## Minimal hex dump

Smallest `.fxr` observed: `N:\GR\data\INTERROOT_win64\sfx\effect\f000002117.fxr`, size `224` bytes.

```text
0000: 46 58 52 00 00 00 05 00 01 00 00 00 45 08 00 00
0010: 90 00 00 00 01 00 00 00 A0 00 00 00 01 00 00 00
0020: B0 00 00 00 00 00 00 00 B0 00 00 00 01 00 00 00
0030: E0 00 00 00 00 00 00 00 E0 00 00 00 00 00 00 00
```
