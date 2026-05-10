/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Implementation of the Binder.* static helpers shared by every
 * BND/BXF format. Mirrors SoulsFormats/Formats/Binder/Binder.cs.
 */

#include "souls_formats/sf_binder.h"

#include <stdio.h>

bool sf_binder_format_has_ids(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_IDS) != 0;
}

bool sf_binder_format_has_names1(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_NAMES1) != 0;
}

bool sf_binder_format_has_names2(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_NAMES2) != 0;
}

bool sf_binder_format_has_long_offsets(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_LONG_OFFSETS) != 0;
}

bool sf_binder_format_has_compression(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_COMPRESSION) != 0;
}

bool sf_binder_format_has_flag6(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_FLAG6) != 0;
}

bool sf_binder_format_has_flag7(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_FLAG7) != 0;
}

bool sf_binder_format_force_big_endian(sf_binder_format_t f) {
    return (f & SF_BINDER_FORMAT_BIG_ENDIAN) != 0;
}

/*  Mirrors Binder.cs:215-228 (BinderTimestampToDate).
 *
 *  Upstream regex:  (\d\d)(\w)(\d+)(\w)(\d+)
 *  scanf analogue:  %2d %c %d %c %d   — digits never bleed into letters,
 *                                       so the greedy %d stops at the next
 *                                       letter naturally.
 *
 *  Range checks are tighter than upstream: upstream relies on the
 *  DateTime constructor to throw on out-of-range values; we explicitly
 *  return SF_ERR_OUT_OF_RANGE for clarity. */
sf_result_t sf_binder_timestamp_parse(const char *timestamp,
                                      sf_binder_datetime_t *out) {
    if (!timestamp || !out) return SF_ERR_INVALID_ARG;

    int  year_offset = 0;
    int  day         = 0;
    int  minute      = 0;
    char month_c     = 0;
    char hour_c      = 0;

    int matched = sscanf(timestamp, "%2d%c%d%c%d",
                         &year_offset, &month_c, &day, &hour_c, &minute);
    if (matched != 5) return SF_ERR_INVALID_ARG;

    if (year_offset < 0 || year_offset > 99) return SF_ERR_OUT_OF_RANGE;

    int month = (int)((unsigned char)month_c) - 'A';
    int hour  = (int)((unsigned char)hour_c)  - 'A';

    if (month < 0 || month > 11) return SF_ERR_OUT_OF_RANGE;
    if (day   < 1 || day   > 31) return SF_ERR_OUT_OF_RANGE;
    if (hour  < 0 || hour  > 23) return SF_ERR_OUT_OF_RANGE;
    if (minute < 0 || minute > 59) return SF_ERR_OUT_OF_RANGE;

    out->year   = 2000 + year_offset;
    out->month  = month;
    out->day    = day;
    out->hour   = hour;
    out->minute = minute;
    return SF_OK;
}

/*  Mirrors Binder.cs:233-245 (DateToBinderTimestamp).
 *
 *  Upstream produces $"{year:D2}{month}{day}{hour}{minute}" then
 *  PadRight(8, '\0'). The visible prefix is at most 8 chars (2 + 1 + 2 +
 *  1 + 2). We use snprintf into the caller's buffer and zero-fill the
 *  remainder to recreate that exact byte sequence. The 9th slot is the
 *  C-string terminator. */
sf_result_t sf_binder_timestamp_format(const sf_binder_datetime_t *dt,
                                       char out_timestamp[9]) {
    if (!dt || !out_timestamp) return SF_ERR_INVALID_ARG;

    int year_offset = dt->year - 2000;
    if (year_offset < 0 || year_offset > 99)        return SF_ERR_OUT_OF_RANGE;
    if (dt->month  < 0 || dt->month  > 11)          return SF_ERR_OUT_OF_RANGE;
    if (dt->day    < 1 || dt->day    > 31)          return SF_ERR_OUT_OF_RANGE;
    if (dt->hour   < 0 || dt->hour   > 23)          return SF_ERR_OUT_OF_RANGE;
    if (dt->minute < 0 || dt->minute > 59)          return SF_ERR_OUT_OF_RANGE;

    char month_c = (char)('A' + dt->month);
    char hour_c  = (char)('A' + dt->hour);

    int n = snprintf(out_timestamp, 9, "%02d%c%d%c%d",
                     year_offset, month_c, dt->day, hour_c, dt->minute);
    if (n < 0 || n > 8) return SF_ERR_OUT_OF_RANGE;

    for (int i = n; i < 9; ++i) {
        out_timestamp[i] = '\0';
    }
    return SF_OK;
}
