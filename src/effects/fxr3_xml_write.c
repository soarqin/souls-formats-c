/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 XML writer (sf_fxr3_t -> mxml DOM -> UTF-8).
 *
 * Mirrors:
 *   SoulsFormats/Formats/FXR3.cs (XmlSerializer annotations)
 *
 * Wave 4 T22 implements the actual serializer.
 */

#include "souls_formats/sf_fxr3.h"

#include "internal/sf_internal.h"

SF_API sf_result_t sf_fxr3_to_xml(const sf_fxr3_t *f, char **out_xml_utf8, size_t *out_size,
                                  const sf_allocator_t *a) {
    (void)f;
    (void)out_xml_utf8;
    (void)out_size;
    (void)a;
    return SF_ERR_INTERNAL; /* Wave 4 T22 implements this */
}
