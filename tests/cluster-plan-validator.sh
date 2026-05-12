#!/usr/bin/env bash
# SHARED-CLUSTER-VALIDATOR — execute this against the cluster file under test.
# Pass argument: the cluster file path, e.g.: .sisyphus/plans/next-batch-legacy-binder.md
#
# Defined in `.sisyphus/plans/refactor-and-gap-analysis.md` §Wave 6 and
# committed by T6.0. Every T6.1-T6.10 cluster plan invokes this same script
# so the F1 final-review gate can re-run it without per-cluster branching.
#
# Usage: bash tests/cluster-plan-validator.sh <cluster-plan.md>
# Exit codes: 0 = PASS, non-zero = FAIL (offending line/section printed to stderr).
set -euo pipefail

CLUSTER_FILE="${1:?usage: cluster-plan-validator.sh <cluster-plan.md>}"
test -f "$CLUSTER_FILE"

# 1) All 9 required sections are present in the file.
for SEC in \
  '^## TL;DR' \
  '^## Upstream formats covered' \
  '^## Must Have' \
  '^## Must NOT Have' \
  '^## Dependencies on prior clusters' \
  '^## Acceptance criteria' \
  '^## STRICT UPSTREAM REFERENCE' \
  '^## Estimated effort' \
  '^## Risk'; do
    grep -qE "$SEC" "$CLUSTER_FILE" || { echo "MISSING $SEC in $CLUSTER_FILE"; exit 1; }
done

# 2) At least one citation of an upstream .cs path inside SoulsFormats/.
grep -qE 'SoulsFormats/.+\.cs' "$CLUSTER_FILE"

# 3) Acceptance criteria block contains at least one executable bash command (lines beginning with
#    a command verb commonly used in our plans).
grep -qE '^\s*(cmake|ctest|grep|test |find |awk|comm|wc|gh|git|x86_64-w64-mingw32-objdump)\b' "$CLUSTER_FILE"

# 4) Cross-coverage: every upstream .cs path this cluster claims to cover is mapped to this cluster
#    in T6.0's evidence inventory (.sisyphus/evidence/upstream-inventory.md).
cluster=$(basename "$CLUSTER_FILE" .md | sed 's/^next-batch-//')
awk -v cluster="$cluster" '
    /^path: SoulsFormats\/.+\.cs/ {p=$2}
    /^cluster: / && p { if ($2==cluster) print p; p="" }
' .sisyphus/evidence/upstream-inventory.md \
  | sort -u > "/tmp/inv-claims-${cluster}.txt"
grep -oE 'SoulsFormats/[A-Za-z0-9_/.]+\.cs' "$CLUSTER_FILE" \
  | sort -u > "/tmp/cluster-cites-${cluster}.txt"
# Cluster file must cite >= 80% of the upstream files inventory assigns to this cluster.
INV=$(wc -l < "/tmp/inv-claims-${cluster}.txt")
CIT=$(comm -12 "/tmp/inv-claims-${cluster}.txt" "/tmp/cluster-cites-${cluster}.txt" | wc -l)
test "$INV" -eq 0 || awk -v inv="$INV" -v cit="$CIT" 'BEGIN { exit !(cit >= 0.8 * inv) }'

echo "VALIDATOR PASS: $CLUSTER_FILE"
