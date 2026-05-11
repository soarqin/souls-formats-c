# Task 4: ER c0000 FLVER2 layout probe

- Source archive: `Data3`
- Source BHD path: `/chr/c0000.chrbnd.dcx`
- Selected FLVER: `N:\GR\data\INTERROOT_win64\chr\c0000\c0000.flver`
- Header version: `0x2001A`
- Endian byte: `0` (little-endian)
- Buffer layout count: `0`
- Mesh count: `0`
- Layout pair count: `0`

## Unique `(Type, Semantic)` pairs

None. The extracted `c0000.flver` in this install is skeleton/dummy-only:
510 dummies, 488 bones, 0 meshes, 0 vertex buffers, and 0 buffer layouts.

## Section skip diagnostics

| Section | Start | Skip | End |
|---|---:|---:|---:|
| header | `0x0` | 128 | `0x80` |
| dummies | `0x80` | 32640 | `0x8000` |
| materials | `0x8000` | 0 | `0x8000` |
| nodes | `0x8000` | 62464 | `0x17400` |
| meshes | `0x17400` | 0 | `0x17400` |
| face_sets | `0x17400` | 0 | `0x17400` |
| vertex_buffers | `0x17400` | 0 | `0x17400` |
| buffer_layout_headers | `0x17400` | 0 | `0x17400` |

Note: this contradicts the task's expected `LAYOUT_PAIR_COUNT >= 5`; the probe now reaches
the Data3 archive successfully, but the actual c0000 FLVER has no vertex layout definitions.
