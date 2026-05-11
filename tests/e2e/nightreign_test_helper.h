/* SPDX-License-Identifier: GPL-3.0-or-later */
/* nightreign_test_helper.h — process-wide singleton for Nightreign e2e tests. */
#ifndef SF_TESTS_NIGHTREIGN_TEST_HELPER_H
#define SF_TESTS_NIGHTREIGN_TEST_HELPER_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>

sf_result_t nightreign_helper_init(void);

sf_result_t nightreign_extract_from_data0(const char *bhd5_path_utf8,
                                          void **out, size_t *out_size);

void nightreign_helper_shutdown(void);

bool nightreign_helper_is_available(void);

#endif /* SF_TESTS_NIGHTREIGN_TEST_HELPER_H */
