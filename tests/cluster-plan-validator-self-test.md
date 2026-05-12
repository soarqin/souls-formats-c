# Self-Test Sample for `cluster-plan-validator.sh`

Minimal hand-written sample that exercises every check in
`tests/cluster-plan-validator.sh`. It is **not** a real cluster plan — its
sole purpose is to anchor the validator's behavior so refactors of the
script can be regression-tested.

The validator derives the cluster name from the filename:
`basename cluster-plan-validator-self-test.md .md` → `cluster-plan-validator-self-test`.
No inventory entry uses that cluster string, so the cross-coverage check (#4)
short-circuits via `INV == 0` and trivially passes.

## TL;DR

This sample contains the **nine required H2 sections** the validator greps
for, plus a single upstream `.cs` citation, plus a runnable bash command in
the Acceptance criteria block — the minimum surface needed to produce a
`VALIDATOR PASS` line.

## Upstream formats covered

Sample citation (any real upstream path works):
`SoulsFormats/Formats/BHD5.cs`.

## Must Have

- A passing run of `tests/cluster-plan-validator.sh` on this file.

## Must NOT Have

- Anything that would falsely match a real cluster name in the inventory.

## Dependencies on prior clusters

None. This file is a leaf self-test invoked by T6.0 only.

## Acceptance criteria

The script must exit 0 when invoked as below:

```bash
bash tests/cluster-plan-validator.sh tests/cluster-plan-validator-self-test.md
```

The acceptance block must also include at least one line whose leading
token is one of: `cmake`, `ctest`, `grep`, `test`, `find`, `awk`, `comm`,
`wc`, `gh`, `git`, `x86_64-w64-mingw32-objdump`. The `grep` invocation
below satisfies that gate:

```bash
grep -qE '^## TL;DR' tests/cluster-plan-validator-self-test.md
```

## STRICT UPSTREAM REFERENCE

Per `AGENTS.md` §5.x, every cluster plan must cite upstream `.cs` paths at
the pinned commit recorded in `docs/api-mapping/UPSTREAM.md`. This sample
cites `SoulsFormats/Formats/BHD5.cs` for that purpose only.

## Estimated effort

Zero — this file is static.

## Risk

Low. If the validator regex set ever changes, regenerate this sample to
match the new contract.
