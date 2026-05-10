/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "compression/compression_internal.h"

/* Chunked EDGE DCX logic lives in dcx.c to keep header parsing colocated with
 * the format dispatcher. This translation unit is reserved for future shared
 * chunk helpers as archive formats begin consuming EDGE blocks directly. */

int sfi_deflate_chunked_translation_unit_anchor(void) {
    return 1;
}
