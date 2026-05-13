# Lighting File Probe — Scope Decisions

## Hit counts

- BTAB:     0
- BTL:      0
- BTPB:     0
- GPARAM:   0
- FLTPARAM: 0
- PMDCL:    0

## Probe coverage notes

The probe scanned:
- ER Data0 (via er_extract_from_data0, hash_37, all candidate paths)
- ER Data1/2/3 (via linear scan with hash_133, confirmed working for MSB files in Data2)
- NR Data0 (via nightreign_extract_from_data0)
- NR data2 (via linear scan with hash_133)
- AC6 Data0-3 (via ac6_extract_from_data0, hash_37)
- Sekiro: SKIP (sekiro_helper_is_available() returned false — likely Oodle init issue)

Key finding: ER Data2 confirmed to use hash_133 (multiplier 133) for non-Data0 shards.
MSB files confirmed present in ER Data2 at /map/mapstudio/<mapid>.msb.dcx.
No lighting files found at any candidate path in any scanned archive.

Likely reasons for 0 hits:
1. Lighting files may use path formats not in our candidate list
2. Sekiro (confirmed BTL/GPARAM user) was not scanned due to helper issue
3. AC6 mapstudio files are in a different shard not accessible via current helpers

## BTPB scope decision

LIGHTING-PROBE: BTPB NOT PRESENT IN V1 GAMES — drop from batch

Decision: DROP — BTPB not found in any v1 game archive. Upstream BTPBVersion enum
terminates at DarkSouls3, consistent with absence in v1 games.

## BTL scope decision

Decision: BTL V18 NOT confirmed (Sekiro not scanned). Implement V16 only (Sekiro V16
is the known version per upstream BTL.cs). V18 hypothesis remains unverified.
Note: BTL.cs says "used in BB, DS3, and Sekiro" — ER/NR/AC6 may not use BTL at all.

## GPARAM extension scope decision

Decision: GPARAM not found in scanned archives, but GPARAM V5 is confirmed by upstream
code to be used in Sekiro and later (including ER/NR). V6 = AC6. Implement as planned.
Both .gparam and .fltparam extensions are documented in upstream GPARAM.cs.

## BTL version breakdown

- V18: NOT confirmed (Sekiro not scanned)
- V16: assumed for Sekiro (per upstream BTL.cs version list)
