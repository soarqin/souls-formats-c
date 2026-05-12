/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_TESTS_DS2S_TEST_HELPER_H
#define SF_TESTS_DS2S_TEST_HELPER_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>

bool ds2s_helper_is_available(void);
sf_result_t ds2s_read_loose_param(const char *param_path, void **out, size_t *out_size);

#endif /* SF_TESTS_DS2S_TEST_HELPER_H */
