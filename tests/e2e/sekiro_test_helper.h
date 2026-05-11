/* SPDX-License-Identifier: GPL-3.0-or-later */
/* sekiro_test_helper.h — process-wide singleton for Sekiro e2e tests.
 * Sekiro ships its dvdbnd as Data1..Data5 (no Data0); the helper opens
 * every shard up front and the extract entry point fans the requested
 * path hash across every open shard, returning the first hit. */
#ifndef SF_TESTS_SEKIRO_TEST_HELPER_H
#define SF_TESTS_SEKIRO_TEST_HELPER_H

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>

/* Initialize all available BHD5 archives (idempotent). Returns SF_OK if
 * at least one shard opened cleanly; otherwise the first non-OK result. */
sf_result_t sekiro_helper_init(void);

/* Extract entry from any of the Sekiro BHD5 archives by path hash.
 * Unwraps an outer DCX wrapper if present. Returns heap-owned bytes via
 * *out (caller frees with sf_free(NULL, ptr)). SF_ERR_NOT_FOUND is
 * propagated only when every open shard returned not-found. */
sf_result_t sekiro_extract_from_anybhd(const char *utf8_path,
                                       void **out, size_t *out_size);

/* Returns true iff every Sekiro BHD5/BDT pair is present on disk. */
bool sekiro_helper_is_available(void);

/* Tear down all open BHD5 handles. atexit-registered after first init. */
void sekiro_helper_shutdown(void);

#endif /* SF_TESTS_SEKIRO_TEST_HELPER_H */
