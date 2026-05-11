/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ac6_test_helper.h — process-wide singleton for AC6 e2e tests. */
#ifndef SF_TESTS_AC6_TEST_HELPER_H
#define SF_TESTS_AC6_TEST_HELPER_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_bhd5.h"

#include <stdbool.h>
#include <stddef.h>

sf_result_t ac6_helper_init(void);

sf_result_t ac6_extract_from_data0(const char *utf8_path, void **out, size_t *out_size);

bool ac6_helper_is_available(void);

void ac6_helper_shutdown(void);

#endif /* SF_TESTS_AC6_TEST_HELPER_H */
