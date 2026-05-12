/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_TESTS_DS1R_TEST_HELPER_H
#define SF_TESTS_DS1R_TEST_HELPER_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>

bool ds1r_helper_is_available(void);
sf_result_t ds1r_read_file(const char *relative_path, void **out, size_t *out_size);
sf_result_t ds1r_extract_bnd3_entry(const char *bnd3_path, const char *entry_suffix,
                                    void **out, size_t *out_size);

#endif /* SF_TESTS_DS1R_TEST_HELPER_H */
