/* SPDX-License-Identifier: GPL-3.0-or-later */
/* er_test_helper.c — placeholder stub; full implementation in T14. */
#include "er_test_helper.h"

sf_result_t er_helper_init(void) { return SF_ERR_INTERNAL; }

sf_result_t er_extract_from_data0(const char *path, void **out, size_t *out_size)
{
    (void)path;
    (void)out;
    (void)out_size;
    return SF_ERR_INTERNAL;
}

void er_helper_shutdown(void) {}

bool er_helper_is_available(void) { return false; }

sf_bhd5_t *er_helper_get_bhd5_for_testing(void) { return NULL; }
