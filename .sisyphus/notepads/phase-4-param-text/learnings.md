2026-05-11: ER phase-4 test helpers can stay test-only in `tests/e2e`; use suffix matching for param entries because DLC-merged paths include extra prefixes.
2026-05-11: For BND payload copies, read the archive into memory, parse with `sf_bnd4_read_from_memory`, then copy the matched entry into a caller-owned buffer.
