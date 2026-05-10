/* SPDX-License-Identifier: GPL-3.0-or-later */
/* er_test_helper.h — process-wide singleton for ER e2e tests.
 * Declarations only. Implementation lands in T14 (er_test_helper.c). */
#ifndef SF_TESTS_ER_TEST_HELPER_H
#define SF_TESTS_ER_TEST_HELPER_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_bhd5.h"  /* for sf_bhd5_t forward decl */
#include <stdbool.h>
#include <stddef.h>

/* Initialize the singleton (idempotent). Returns SF_OK or propagated error. */
sf_result_t er_helper_init(void);

/* Extract entry by BHD5 path. Auto-decompresses outer DCX wrapper.
 * Returns heap-owned bytes via *out (caller frees via sf_free).
 * Returns error code (not necessarily FAIL) if env missing. */
sf_result_t er_extract_from_data0(const char *bhd5_path_utf8,
                                  void **out, size_t *out_size);

/* Load a regulation param BND entry by suffix match. Caller owns *out_bytes. */
sf_result_t er_load_param(const char *param_name, void **out_bytes,
                          size_t *out_size, const sf_allocator_t *alloc);

/* Load a msgbnd entry by substring match. Caller owns *out_bytes. */
sf_result_t er_load_msgbnd_entry(const char *msgbnd_path, const char *entry_name,
                                 void **out_bytes, size_t *out_size,
                                 const sf_allocator_t *alloc);

/* Tear down. atexit-registered. */
void er_helper_shutdown(void);

/* Returns true iff er_extract_from_data0 can work in this environment. */
bool er_helper_is_available(void);

/* Test-only accessor: returns the underlying sf_bhd5_t* (NULL if not init). */
sf_bhd5_t *er_helper_get_bhd5_for_testing(void);

#endif /* SF_TESTS_ER_TEST_HELPER_H */
