# F3 Final QA Results

Date: 2026-05-13T22:20:49
Working directory: `/home/soar/src/souls-formats-c`
Verdict: **APPROVE**

## Summary

- Clean rebuild warning/error count: `0`
- Lighting tests: 8/8 PASS
- Lighting e2e skip behavior: 4/4 tested, all emit informative `IGNORE` messages and pass in CTest
- Probe gating: default build absent; `SF_BUILD_PROBES=ON` build present
- Lighting exports: `105` matching `sf_(btab|btl|gparam|pmdcl)_` symbols (>= 30 required)
- Full regression: 182/182 PASS

## 1. Clean rebuild

Command:
```bash
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw 2>&1 | grep -cE 'warning:|error:'
```

Result:
```text
0
```

## 2. Run all lighting tests

Command:
```bash
ctest --test-dir build-mingw -L lighting --output-on-failure 2>&1
```

Key output:
```text
1/8 Test #166: souls_formats_test_pmdcl_synthetic ....   Passed    0.14 sec
2/8 Test #167: souls_formats_test_btab_synthetic .....   Passed    0.14 sec
3/8 Test #168: souls_formats_test_btl_synthetic ......   Passed    0.13 sec
4/8 Test #169: souls_formats_test_gparam_synthetic ...   Passed    0.17 sec
5/8 Test #170: souls_formats_test_pmdcl_e2e ..........   Passed    0.47 sec
6/8 Test #171: souls_formats_test_btab_e2e ...........   Passed    0.47 sec
7/8 Test #172: souls_formats_test_btl_e2e ............   Passed    0.58 sec
8/8 Test #173: souls_formats_test_gparam_e2e .........   Passed    0.70 sec
100% tests passed, 0 tests failed out of 8
```

## 3. Verify e2e graceful skip behavior

Command:
```bash
ctest --test-dir build-mingw -L lighting -R '_e2e' --output-on-failure -V 2>&1 | grep -E "SKIP|IGNORE|PASS|FAIL"
```

Output:
```text
170: /home/soar/src/souls-formats-c/tests/lighting/test_pmdcl_e2e.c:104:test_pmdcl_e2e_multi_game:IGNORE: pmdcl e2e: no .pmdcl entries found in Data0 across ER/AC6/NR/Sekiro (Wave-0 probe confirmed 0; PMDCL files likely live in non-Data0 shards or are absent from v1 games)
171: /home/soar/src/souls-formats-c/tests/lighting/test_btab_e2e.c:104:test_btab_e2e_multi_game:IGNORE: btab e2e: no .btab entries found in Data0 across ER/AC6/NR/Sekiro (Wave-0 probe confirmed 0; BTAB files likely live in non-Data0 shards or under unmatched paths)
172: /home/soar/src/souls-formats-c/tests/lighting/test_btl_e2e.c:104:test_btl_e2e_multi_game:IGNORE: btl e2e: no .btl entries found in Data0 across ER/AC6/NR/Sekiro (Wave-0 probe confirmed 0; upstream notes BTL is BB/DS3/Sekiro-only \xE2\x80\x94 ER/NR/AC6 may not ship BTL at all)
173: /home/soar/src/souls-formats-c/tests/lighting/test_gparam_e2e.c:112:test_gparam_e2e_multi_game:IGNORE: gparam e2e: no .gparam/.fltparam entries found in Data0 across ER/AC6/NR/Sekiro (Wave-0 probe confirmed 0; GPARAM files likely live in non-Data0 shards or under unmatched paths)
```

## 4. Verify probe binary gating

Commands:
```bash
ls build-mingw/tests/probes/probe_lighting_files.exe 2>/dev/null && echo "PROBE IN DEFAULT BUILD (WRONG)" || echo "Probe absent from default build (correct)"
cmake -B build-probe -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_PROBES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-probe --target probe_lighting_files 2>&1 | tail -3
ls build-probe/tests/probes/probe_lighting_files.exe && echo "Probe binary exists (correct)"
```

Key output:
```text
Probe absent from default build (correct)
build-probe/tests/probes/probe_lighting_files.exe
Probe binary exists (correct)
```

## 5. Verify symbol exports

Command:
```bash
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -cE 'sf_(btab|btl|gparam|pmdcl)_'
```

Result:
```text
105
```

## 6. Verify no regressions

Command:
```bash
ctest --test-dir build-mingw --output-on-failure 2>&1 | grep -E "tests passed|FAILED"
```

Output:
```text
100% tests passed, 0 tests failed out of 182
```
