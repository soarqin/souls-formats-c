/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TAE event-template public surface.
 *
 * Mirrors upstream SoulsFormats/Formats/TAE/Template.cs. Templates describe
 * typed event parameter layouts for a TAE event bank.
 */

#ifndef SOULS_FORMATS_SF_TAE_TEMPLATE_H
#define SOULS_FORMATS_SF_TAE_TEMPLATE_H

#include "sf_common.h"
#include "souls_formats/sf_tae.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_tae_param_type {
    SF_TAE_PARAM_TYPE_B       = 0,  /* bool (1 byte) */
    SF_TAE_PARAM_TYPE_U8      = 1,  /* unsigned byte */
    SF_TAE_PARAM_TYPE_X8      = 2,  /* unsigned byte, display as hex */
    SF_TAE_PARAM_TYPE_S8      = 3,  /* signed byte */
    SF_TAE_PARAM_TYPE_U16     = 4,
    SF_TAE_PARAM_TYPE_X16     = 5,
    SF_TAE_PARAM_TYPE_S16     = 6,
    SF_TAE_PARAM_TYPE_U32     = 7,
    SF_TAE_PARAM_TYPE_X32     = 8,
    SF_TAE_PARAM_TYPE_S32     = 9,
    SF_TAE_PARAM_TYPE_U64     = 10,
    SF_TAE_PARAM_TYPE_X64     = 11,
    SF_TAE_PARAM_TYPE_S64     = 12,
    SF_TAE_PARAM_TYPE_F32     = 13,
    SF_TAE_PARAM_TYPE_F32GRAD = 14, /* two f32 values (8 bytes) */
    SF_TAE_PARAM_TYPE_F64     = 15,
    SF_TAE_PARAM_TYPE_AOB     = 16, /* array of bytes, length in aob_length */
} sf_tae_param_type_t;

_Static_assert(SF_TAE_PARAM_TYPE_AOB == 16, "sf_tae_param_type_t drift");

typedef struct sf_tae_template       sf_tae_template_t;
typedef struct sf_tae_bank_template  sf_tae_bank_template_t;
typedef struct sf_tae_event_template sf_tae_event_template_t;
typedef struct sf_tae_param_template sf_tae_param_template_t;

SF_API sf_result_t sf_tae_template_read_from_file(sf_tae_template_t **out,
                                                  const wchar_t *path,
                                                  const sf_allocator_t *a);
SF_API sf_result_t sf_tae_template_read_from_memory(sf_tae_template_t **out,
                                                    const char *xml_text, size_t xml_len,
                                                    const sf_allocator_t *a);
SF_API void sf_tae_template_destroy(sf_tae_template_t *t);

SF_API sf_tae_format_t sf_tae_template_game(const sf_tae_template_t *t);
SF_API size_t sf_tae_template_bank_count(const sf_tae_template_t *t);
SF_API const sf_tae_bank_template_t *sf_tae_template_find_bank(const sf_tae_template_t *t,
                                                               int64_t bank_id);
SF_API const sf_tae_bank_template_t *sf_tae_template_bank(const sf_tae_template_t *t,
                                                          size_t i);

SF_API int64_t sf_tae_bank_template_id(const sf_tae_bank_template_t *b);
SF_API const char *sf_tae_bank_template_name(const sf_tae_bank_template_t *b);
SF_API size_t sf_tae_bank_template_event_count(const sf_tae_bank_template_t *b);
SF_API const sf_tae_event_template_t *sf_tae_bank_template_find_event(
    const sf_tae_bank_template_t *b, int32_t event_id);
SF_API const sf_tae_event_template_t *sf_tae_bank_template_event(
    const sf_tae_bank_template_t *b, size_t i);

SF_API int32_t sf_tae_event_template_id(const sf_tae_event_template_t *e);
SF_API const char *sf_tae_event_template_name(const sf_tae_event_template_t *e);
SF_API size_t sf_tae_event_template_param_count(const sf_tae_event_template_t *e);
SF_API int32_t sf_tae_event_template_total_byte_count(const sf_tae_event_template_t *e);
SF_API const sf_tae_param_template_t *sf_tae_event_template_param(
    const sf_tae_event_template_t *e, size_t i);
SF_API const sf_tae_param_template_t *sf_tae_event_template_find_param(
    const sf_tae_event_template_t *e, const char *key);

SF_API sf_tae_param_type_t sf_tae_param_template_type(const sf_tae_param_template_t *p);
SF_API const char *sf_tae_param_template_name(const sf_tae_param_template_t *p);
SF_API const char *sf_tae_param_template_name_group(const sf_tae_param_template_t *p);
SF_API const char *sf_tae_param_template_key(const sf_tae_param_template_t *p);
SF_API int32_t sf_tae_param_template_byte_count(const sf_tae_param_template_t *p);
SF_API int32_t sf_tae_param_template_aob_length(const sf_tae_param_template_t *p);
SF_API bool sf_tae_param_template_has_assert(const sf_tae_param_template_t *p);
SF_API bool sf_tae_param_template_has_default(const sf_tae_param_template_t *p);
SF_API size_t sf_tae_param_template_enum_count(const sf_tae_param_template_t *p);
SF_API const char *sf_tae_param_template_enum_name(const sf_tae_param_template_t *p,
                                                   size_t i);
SF_API int64_t sf_tae_param_template_enum_value(const sf_tae_param_template_t *p,
                                                size_t i);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_TAE_TEMPLATE_H */
