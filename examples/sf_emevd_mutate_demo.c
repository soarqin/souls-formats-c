/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>

static int fail(const char *what, sf_result_t r) {
    fprintf(stderr, "%s: %s\n", what, sf_result_str(r));
    return 1;
}

int main(void) {
    sf_emevd_t *emevd = NULL;
    sf_result_t r = sf_emevd_create(NULL, SF_EMEVD_FORMAT_ELDEN_RING, &emevd);
    if (r != SF_OK) return fail("create EMEVD", r);

    sf_emevd_event_t *event = NULL;
    r = sf_emevd_add_event(emevd, 279551111, SF_EMEVD_REST_BEHAVIOR_DEFAULT, &event);
    if (r != SF_OK) {
        sf_emevd_destroy(emevd, NULL);
        return fail("add Open Graces event", r);
    }

    const uint8_t end_if_flag_args[] = {0, 1, 0, 0xC4, 0xEA, 0x00, 0x00};
    r = sf_emevd_event_insert_instruction(event, 0, 1003, 2, end_if_flag_args,
                                          sizeof(end_if_flag_args));
    if (r != SF_OK) {
        sf_emevd_destroy(emevd, NULL);
        return fail("insert instruction", r);
    }

    r = sf_emevd_event_clear_parameters(event);
    if (r != SF_OK) {
        sf_emevd_destroy(emevd, NULL);
        return fail("clear parameters", r);
    }

    uint8_t *bytes = NULL;
    size_t size = 0;
    r = sf_emevd_write_to_memory(emevd, &bytes, &size, NULL);
    if (r != SF_OK) {
        sf_emevd_destroy(emevd, NULL);
        return fail("write EMEVD", r);
    }

    printf("mutated EMEVD image: %zu bytes\n", size);
    sf_free(NULL, bytes);
    sf_emevd_destroy(emevd, NULL);
    return 0;
}
