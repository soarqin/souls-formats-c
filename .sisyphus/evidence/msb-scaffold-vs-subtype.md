# T0.5 — MSB Scaffolding-vs-Subtype LOC Breakdown

## Verdict: GO (~50-100 LOC extractable scaffolding per module)

## File LOC Breakdown

| File | Total LOC | Est. Scaffolding LOC | Est. Subtype LOC | Shareable? |
|------|-----------|---------------------|------------------|------------|
| src/map/msbs/parts_param.c | 627 | ~80 | ~547 | Y (scaffold only) |
| src/map/msbs/point_param.c | 524 | ~80 | ~444 | Y |
| src/map/msbs/event_param.c | 440 | ~80 | ~360 | Y |
| src/map/msbs/msbs.c | 275 | ~120 | ~155 | Y (entry-list engine) |
| src/map/msbs/model_param.c | 259 | ~80 | ~179 | Y |
| src/map/msbs/route_param.c | 152 | ~60 | ~92 | Y |
| src/map/msbe/msbe.c | 261 | ~120 | ~141 | Y (entry-list engine) |
| src/map/msbe/event_param.c | 192 | ~60 | ~132 | Y |
| src/map/msbe/model_param.c | 181 | ~60 | ~121 | Y |
| src/map/msbe/point_param.c | 165 | ~60 | ~105 | Y |
| src/map/msbe/parts_param.c | 132 | ~50 | ~82 | Y |
| src/map/msbe/route_param.c | 107 | ~50 | ~57 | Y |
| src/map/msbvi/msbvi.c | 248 | ~100 | ~148 | Y (entry-list engine) |
| src/map/msbvi/event_param.c | 131 | ~50 | ~81 | Y |
| src/map/msbvi/model_param.c | 128 | ~50 | ~78 | Y |
| src/map/msbvi/parts_param.c | 99 | ~40 | ~59 | Y (minimal) |
| src/map/msbvi/point_param.c | 98 | ~40 | ~58 | Y (minimal) |
| src/map/msbvi/layer_param.c | 84 | ~40 | ~44 | Y (minimal) |
| src/map/msbvi/route_param.c | 72 | ~40 | ~32 | Y (minimal) |

**Total MSB LOC**: 4,175
**Estimated extractable scaffolding**: ~1,100 LOC (26%)

## Module Aggregates

| Module | Total LOC | Scaffolding LOC | Subtype LOC | % Extractable |
|--------|-----------|-----------------|-------------|---------------|
| msbs (6 files) | 2,277 | ~500 | ~1,777 | ~22% |
| msbe (6 files) | 1,038 | ~400 | ~638 | ~39% |
| msbvi (7 files) | 860 | ~360 | ~500 | ~42% |

## Metis Claim Validation

`parts_param.c` LOC: msbs=627, msbe=132, msbvi=99.
The 627→132→99 difference is confirmed to be **mostly per-subtype fields**:
- msbs has Sekiro-specific subtypes (MapPiece, Object, Enemy, Player, Collision, etc.) with
  many more per-subtype field reads than msbe/msbvi
- The scaffolding (offset table read, count check, alloc loop, index backfill) is ~50-80 LOC
  in all three variants — the divergence is in the subtype dispatch tables

## Scaffolding Definition (for Wave 5)

Extractable scaffolding = the following pattern (present in all 19 files):
1. Reserve "NextList<N>" offset
2. Fill name offset
3. Write type name string (UTF-16)
4. Pad to alignment
5. For each entry: call per-entry write function
6. Fill "NextList<N>" with current position

This is ~50-100 LOC per file. The per-entry write functions contain the subtype-specific
field reads and are NOT extractable.

## Downstream Impact

- **T5.1**: GO — extract `msb_entry_list_read` / `msb_entry_list_write` into `msb_common.c`
  - Target: ~50-100 LOC extracted per file
  - Limit: scaffolding only; per-subtype field reads stay in place
- **T5.2**: GO — apply to all 19 files (3 sub-PRs: msbs, msbe, msbvi)
- Expected LOC reduction: ~15-25% of total MSB LOC (within the 15-25% target)
