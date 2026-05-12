/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 XML reader (mxml DOM -> sf_fxr3_t).
 *
 * Mirrors:
 *   SoulsFormats/Formats/FXR3.cs (XmlSerializer annotations)
 *
 * Wave 4 T21 implements the actual deserializer.
 */

#include "souls_formats/sf_fxr3.h"

#include "internal/sf_internal.h"

SF_API sf_result_t sf_fxr3_from_xml(sf_fxr3_t **out, const char *xml_utf8, size_t xml_size,
                                    const sf_allocator_t *a) {
    (void)xml_utf8;
    (void)xml_size;
    (void)a;
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    return SF_ERR_INTERNAL; /* Wave 4 T21 implements this */
}
