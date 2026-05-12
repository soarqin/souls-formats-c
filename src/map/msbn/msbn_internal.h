/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBN_INTERNAL_H
#define SF_MAP_MSBN_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbn.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbn_model_type { MSBN_MODEL_MAP_PIECE = 1 } msbn_model_type_t;
typedef enum msbn_part_type { MSBN_PART_MAP_PIECE = 1 } msbn_part_type_t;

typedef struct msbn_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    const sf_allocator_t *alloc;
} msbn_model_t;

typedef struct msbn_part {
    msbn_part_type_t     type;
    char                *name;
    int32_t              model_index;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    const sf_allocator_t *alloc;
} msbn_part_t;

struct sf_msbn_model { msbn_model_t data; };
struct sf_msbn_part  { msbn_part_t data; };

struct sf_msbn {
    const sf_allocator_t *alloc;
    sf_msbn_model_t *models;
    int32_t          model_count;
    sf_msbn_part_t  *parts;
    int32_t          part_count;
};

void msbn_model_param_free(sf_msbn_model_t *models, int32_t count, const sf_allocator_t *a);
void msbn_parts_param_free(sf_msbn_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbn_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbn_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbn_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbn_t *out,
                                  const sf_allocator_t *a);

sf_result_t msbn_model_param_write(sf_binary_writer_t *w, const sf_msbn_t *msbn);
sf_result_t msbn_parts_param_write(sf_binary_writer_t *w, const sf_msbn_t *msbn);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBN_INTERNAL_H */
