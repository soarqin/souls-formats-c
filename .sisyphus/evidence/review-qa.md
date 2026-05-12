# F3 — Real Manual QA — Wave Acceptance Gates

Date: 2026-05-12
Reviewer: Sisyphus-Junior (QA)
Working dir: `/home/soar/src/souls-formats-c`

## Summary Line

```
Consumer-build [PASS] | BUILD_TESTING toggle [PASS] | Golden [PASS] | e2e skip-count [STABLE] | Cluster plans [10/10 PASS] | VERDICT: APPROVE
```

---

## Check 1 — Consumer build (no tests built)

Command:
```bash
cmake -B /tmp/sf-consumer/build -G Ninja \
  --toolchain /home/soar/src/souls-formats-c/cmake/toolchain-mingw-w64.cmake \
  /tmp/sf-consumer
```

Output:
```
--   Top-level project : OFF
--   Build tests       : OFF
```

Build:
```
ninja: no work to do.
```

Tests dir check:
```
CONSUMER OK: no tests dir
```
`/tmp/sf-consumer/build/sf/` contents: `CMakeFiles`, `cmake_install.cmake`,
`mxml-config` — no `tests/` directory.

**Result: PASS**

---

## Check 2 — BUILD_TESTING=OFF (library only)

Command:
```bash
cmake -B /tmp/build-off -G Ninja \
  --toolchain cmake/toolchain-mingw-w64.cmake \
  -DBUILD_TESTING=OFF .
```

Output:
```
--   Top-level project : ON
--   Build tests       : OFF
```

**Result: PASS** — BUILD_TESTING=OFF correctly disables tests at top level.

---

## Check 3 — SF_BUILD_TESTS deprecation alias

Command:
```bash
cmake -B /tmp/build-legacy -G Ninja \
  --toolchain cmake/toolchain-mingw-w64.cmake \
  -DSF_BUILD_TESTS=OFF .
```

Output:
```
CMake Deprecation Warning at CMakeLists.txt:19 (message):
  SF_BUILD_TESTS is deprecated; use -DBUILD_TESTING=ON/OFF instead.
--   Build tests       : OFF
```

**Result: PASS** — deprecation warning is emitted, value forwarded to BUILD_TESTING.

---

## Check 4 — Golden hashes

Command:
```bash
ctest --test-dir build-mingw -L golden --output-on-failure
```

Output:
```
    Start 103: souls_formats_test_golden
1/1 Test #103: souls_formats_test_golden ........   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1
```

**Result: PASS** — 1/1 golden tests pass.

---

## Check 5 — e2e skip count stable

Command:
```bash
ctest --test-dir build-mingw -L 'e2e' -V --output-on-failure 2>&1 \
  | grep -cE 'SKIP:|gracefully skipping|TEST_IGNORE_MESSAGE' > /tmp/skip-now.txt
diff .sisyphus/evidence/skip-count-baseline.txt /tmp/skip-now.txt
```

Result:
- Baseline: `0`
- Current:  `0`
- Diff: empty

```
SKIP COUNT STABLE
```

**Result: STABLE**

---

## Check 6 — Cluster plan validator on all 10 plans

Command (loop):
```bash
for f in .sisyphus/plans/next-batch-*.md; do
  bash tests/cluster-plan-validator.sh "$f" 2>&1 | tail -1
done
```

Per-plan results:
```
next-batch-ac-specific.md: VALIDATOR PASS
next-batch-effects-misc.md: VALIDATOR PASS
next-batch-legacy-binder.md: VALIDATOR PASS
next-batch-legacy-flver.md: VALIDATOR PASS
next-batch-legacy-msb.md: VALIDATOR PASS
next-batch-lighting.md: VALIDATOR PASS
next-batch-navmesh.md: VALIDATOR PASS
next-batch-tae-templates.md: VALIDATOR PASS
next-batch-text-script-misc.md: VALIDATOR PASS
next-batch-uncategorized-deferred.md: VALIDATOR PASS
```

Totals: **10/10 PASS, 0 FAIL**

**Result: 10/10 PASS**

---

## Check 7 — SF_BUILD_PROBES=OFF (no probes)

Configure:
```
--   Sanitizers        : OFF
-- Configuring done (27.6s)
-- Generating done (0.2s)
-- Build files have been written to: /tmp/build-noprobes
```

Build (after recovery from a transient DLL-copy race; second invocation
completed cleanly, 10/10 remaining link steps succeeded):
```
[10/10] Linking ... DLL to tests/e2e_er/
```

Probe check:
```
PROBES OFF OK: probe_nightreign_msb.exe NOT BUILT
ls: cannot access '/tmp/build-noprobes/tests/probes/': No such file or directory
```

`/tmp/build-noprobes/tests/` contents do not include a `probes/` directory.

**Result: PASS** — SF_BUILD_PROBES=OFF correctly skips probe targets.

---

## Verdict

All 7 gates passed:

| # | Gate | Result |
|---|------|--------|
| 1 | Consumer build (no tests built) | PASS |
| 2 | BUILD_TESTING=OFF (library only) | PASS |
| 3 | SF_BUILD_TESTS deprecation alias | PASS |
| 4 | Golden hashes | PASS |
| 5 | e2e skip count stable | STABLE (0 == 0) |
| 6 | Cluster plan validator on 10 plans | 10/10 PASS |
| 7 | SF_BUILD_PROBES=OFF | PASS |

**VERDICT: APPROVE**
