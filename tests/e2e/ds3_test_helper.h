/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_TESTS_DS3_TEST_HELPER_H
#define SF_TESTS_DS3_TEST_HELPER_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>

sf_result_t ds3_helper_init(void);
bool ds3_helper_is_available(void);
sf_result_t ds3_extract_from_anybhd(const char *utf8_path, void **out, size_t *out_size);
void ds3_helper_shutdown(void);

#endif /* SF_TESTS_DS3_TEST_HELPER_H */
