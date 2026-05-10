/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Encoding conversion helpers backed by Win32 MultiByteToWideChar /
 * WideCharToMultiByte. Inputs/outputs:
 *
 *     UTF-8       (CP_UTF8 = 65001)
 *     ASCII       (CP_ACP with high bytes treated as '?')
 *     Shift-JIS   (CP 932)
 *     UTF-16 LE   (Win32 WCHAR is UTF-16 LE)
 *     UTF-16 BE   (LE with byte-swap)
 */

#include "souls_formats/sf_encoding.h"

#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#define SF_CP_SHIFT_JIS 932

/*---------------------------------------------------------------------------
 * Shared helper: MBCS bytes (in `cp`) → UTF-8.
 *---------------------------------------------------------------------------*/
static sf_result_t mbcs_to_utf8(UINT cp, const void *in, size_t in_size,
                                char **out_utf8, size_t *out_len_bytes,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out_utf8 != NULL);
    *out_utf8 = NULL;
    if (out_len_bytes) *out_len_bytes = 0;

    if (in_size == 0) {
        char *empty = (char *)sf_xalloc(a, 1);
        if (!empty) return SF_ERR_OOM;
        empty[0] = '\0';
        *out_utf8 = empty;
        return SF_OK;
    }
    SF_CHECK_ARG(in != NULL);

    /*  in_size fits int? Win32 MBCS APIs use int. */
    if (in_size > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    int wlen = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS,
                                   (const char *)in, (int)in_size, NULL, 0);
    if (wlen <= 0) return SF_ERR_INVALID_ARG;

    wchar_t *wbuf = (wchar_t *)sf_xalloc(a, (size_t)wlen * sizeof(wchar_t));
    if (!wbuf) return SF_ERR_OOM;

    int got = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS,
                                  (const char *)in, (int)in_size, wbuf, wlen);
    if (got != wlen) {
        sf_xfree(a, wbuf);
        return SF_ERR_INVALID_ARG;
    }

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen,
                                    NULL, 0, NULL, NULL);
    if (u8len <= 0) {
        sf_xfree(a, wbuf);
        return SF_ERR_INVALID_ARG;
    }

    char *out = (char *)sf_xalloc(a, (size_t)u8len + 1);
    if (!out) {
        sf_xfree(a, wbuf);
        return SF_ERR_OOM;
    }

    int u8got = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen,
                                    out, u8len, NULL, NULL);
    sf_xfree(a, wbuf);
    if (u8got != u8len) {
        sf_xfree(a, out);
        return SF_ERR_INVALID_ARG;
    }

    out[u8len] = '\0';
    *out_utf8 = out;
    if (out_len_bytes) *out_len_bytes = (size_t)u8len;
    return SF_OK;
}

/*---------------------------------------------------------------------------
 * Shared helper: UTF-8 → wide → MBCS.
 *---------------------------------------------------------------------------*/
static sf_result_t utf8_to_wide(const char *utf8, size_t utf8_len,
                                wchar_t **out, int *out_wlen,
                                const sf_allocator_t *a) {
    if (utf8_len == 0) {
        *out = NULL;
        *out_wlen = 0;
        return SF_OK;
    }
    if (utf8_len > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   utf8, (int)utf8_len, NULL, 0);
    if (wlen <= 0) return SF_ERR_INVALID_ARG;

    wchar_t *wbuf = (wchar_t *)sf_xalloc(a, (size_t)wlen * sizeof(wchar_t));
    if (!wbuf) return SF_ERR_OOM;

    int got = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  utf8, (int)utf8_len, wbuf, wlen);
    if (got != wlen) {
        sf_xfree(a, wbuf);
        return SF_ERR_INVALID_ARG;
    }
    *out = wbuf;
    *out_wlen = wlen;
    return SF_OK;
}

static sf_result_t utf8_to_mbcs(UINT cp, const char *utf8, bool terminate,
                                void **out, size_t *out_size_bytes,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (out_size_bytes) *out_size_bytes = 0;
    if (!utf8) utf8 = "";

    size_t u8len = strlen(utf8);

    wchar_t *wbuf = NULL;
    int wlen = 0;
    sf_result_t r = utf8_to_wide(utf8, u8len, &wbuf, &wlen, a);
    if (r != SF_OK) return r;

    int mblen = 0;
    if (wlen > 0) {
        mblen = WideCharToMultiByte(cp, 0, wbuf, wlen, NULL, 0, NULL, NULL);
        if (mblen <= 0) {
            sf_xfree(a, wbuf);
            return SF_ERR_INVALID_ARG;
        }
    }

    size_t total = (size_t)mblen + (terminate ? 1 : 0);
    if (total == 0) {
        /*  Caller wants empty output; still allocate 1 byte of '\0' for
         *  ergonomic NUL-terminated consumption. */
        char *empty = (char *)sf_xalloc(a, 1);
        if (!empty) {
            sf_xfree(a, wbuf);
            return SF_ERR_OOM;
        }
        empty[0] = '\0';
        *out = empty;
        if (out_size_bytes) *out_size_bytes = 0;
        sf_xfree(a, wbuf);
        return SF_OK;
    }

    char *buf = (char *)sf_xalloc(a, total);
    if (!buf) {
        sf_xfree(a, wbuf);
        return SF_ERR_OOM;
    }

    if (mblen > 0) {
        int got = WideCharToMultiByte(cp, 0, wbuf, wlen, buf, mblen, NULL, NULL);
        if (got != mblen) {
            sf_xfree(a, wbuf);
            sf_xfree(a, buf);
            return SF_ERR_INVALID_ARG;
        }
    }
    if (terminate) buf[mblen] = '\0';

    sf_xfree(a, wbuf);
    *out = buf;
    if (out_size_bytes) *out_size_bytes = total;
    return SF_OK;
}

/*---------------------------------------------------------------------------
 * UTF-16 LE / BE → UTF-8.
 *---------------------------------------------------------------------------*/
static sf_result_t utf16_to_utf8(bool big_endian,
                                 const void *in, size_t in_size,
                                 char **out_utf8, size_t *out_len_bytes,
                                 const sf_allocator_t *a) {
    SF_CHECK_ARG(out_utf8 != NULL);
    *out_utf8 = NULL;
    if (out_len_bytes) *out_len_bytes = 0;

    /*  Mirror upstream behaviour: an odd byte count drops the trailing byte. */
    size_t code_units = in_size / 2;

    if (code_units == 0) {
        char *empty = (char *)sf_xalloc(a, 1);
        if (!empty) return SF_ERR_OOM;
        empty[0] = '\0';
        *out_utf8 = empty;
        return SF_OK;
    }
    SF_CHECK_ARG(in != NULL);
    if (code_units > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    /*  Win32 wchar_t is UTF-16 LE. For BE input we copy + byte-swap. */
    const wchar_t *wsrc;
    wchar_t *wbuf_owned = NULL;

    if (big_endian) {
        wbuf_owned = (wchar_t *)sf_xalloc(a, code_units * sizeof(wchar_t));
        if (!wbuf_owned) return SF_ERR_OOM;
        const uint8_t *bytes = (const uint8_t *)in;
        for (size_t i = 0; i < code_units; i++) {
            uint16_t hi = bytes[i * 2 + 0];
            uint16_t lo = bytes[i * 2 + 1];
            wbuf_owned[i] = (wchar_t)((hi << 8) | lo);
        }
        wsrc = wbuf_owned;
    } else {
        wsrc = (const wchar_t *)in;
    }

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wsrc, (int)code_units,
                                    NULL, 0, NULL, NULL);
    if (u8len <= 0) {
        sf_xfree(a, wbuf_owned);
        return SF_ERR_INVALID_ARG;
    }

    char *out = (char *)sf_xalloc(a, (size_t)u8len + 1);
    if (!out) {
        sf_xfree(a, wbuf_owned);
        return SF_ERR_OOM;
    }

    int got = WideCharToMultiByte(CP_UTF8, 0, wsrc, (int)code_units,
                                  out, u8len, NULL, NULL);
    sf_xfree(a, wbuf_owned);
    if (got != u8len) {
        sf_xfree(a, out);
        return SF_ERR_INVALID_ARG;
    }

    out[u8len] = '\0';
    *out_utf8 = out;
    if (out_len_bytes) *out_len_bytes = (size_t)u8len;
    return SF_OK;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

sf_result_t sf_ascii_to_utf8(const void *in, size_t in_size,
                             char **out_utf8, size_t *out_len_bytes,
                             const sf_allocator_t *a) {
    /*  Treat ASCII as bytes 0..127. Any byte > 127 maps via current ANSI
     *  code page, which matches .NET Encoding.ASCII's permissive behaviour
     *  of producing '?' for out-of-range bytes. We use code page 20127
     *  (us-ascii) which Windows ships universally. */
    SF_CHECK_ARG(out_utf8 != NULL);
    *out_utf8 = NULL;
    if (out_len_bytes) *out_len_bytes = 0;

    if (in_size == 0) {
        char *empty = (char *)sf_xalloc(a, 1);
        if (!empty) return SF_ERR_OOM;
        empty[0] = '\0';
        *out_utf8 = empty;
        return SF_OK;
    }
    SF_CHECK_ARG(in != NULL);

    /*  Fast path: pure ASCII passes straight through as UTF-8. */
    const uint8_t *bytes = (const uint8_t *)in;
    bool ascii_clean = true;
    for (size_t i = 0; i < in_size; i++) {
        if (bytes[i] >= 0x80) { ascii_clean = false; break; }
    }

    if (ascii_clean) {
        char *out = (char *)sf_xalloc(a, in_size + 1);
        if (!out) return SF_ERR_OOM;
        memcpy(out, in, in_size);
        out[in_size] = '\0';
        *out_utf8 = out;
        if (out_len_bytes) *out_len_bytes = in_size;
        return SF_OK;
    }

    /*  Fall back to code page 20127 which substitutes '?' for non-ASCII. */
    return mbcs_to_utf8(20127u, in, in_size, out_utf8, out_len_bytes, a);
}

sf_result_t sf_shift_jis_to_utf8(const void *in, size_t in_size,
                                 char **out_utf8, size_t *out_len_bytes,
                                 const sf_allocator_t *a) {
    return mbcs_to_utf8(SF_CP_SHIFT_JIS, in, in_size, out_utf8, out_len_bytes, a);
}

sf_result_t sf_utf16le_to_utf8(const void *in, size_t in_size,
                               char **out_utf8, size_t *out_len_bytes,
                               const sf_allocator_t *a) {
    return utf16_to_utf8(false, in, in_size, out_utf8, out_len_bytes, a);
}

sf_result_t sf_utf16be_to_utf8(const void *in, size_t in_size,
                               char **out_utf8, size_t *out_len_bytes,
                               const sf_allocator_t *a) {
    return utf16_to_utf8(true, in, in_size, out_utf8, out_len_bytes, a);
}

sf_result_t sf_utf8_to_ascii(const char *utf8, bool terminate,
                             void **out, size_t *out_size_bytes,
                             const sf_allocator_t *a) {
    return utf8_to_mbcs(20127u, utf8, terminate, out, out_size_bytes, a);
}

sf_result_t sf_utf8_to_shift_jis(const char *utf8, bool terminate,
                                 void **out, size_t *out_size_bytes,
                                 const sf_allocator_t *a) {
    return utf8_to_mbcs(SF_CP_SHIFT_JIS, utf8, terminate, out, out_size_bytes, a);
}

sf_result_t sf_utf8_to_utf16le(const char *utf8, bool terminate,
                               void **out, size_t *out_size_bytes,
                               const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (out_size_bytes) *out_size_bytes = 0;
    if (!utf8) utf8 = "";

    size_t u8len = strlen(utf8);
    wchar_t *wbuf = NULL;
    int wlen = 0;
    sf_result_t r = utf8_to_wide(utf8, u8len, &wbuf, &wlen, a);
    if (r != SF_OK) return r;

    size_t total_units = (size_t)wlen + (terminate ? 1u : 0u);
    size_t total_bytes = total_units * 2u;
    if (total_bytes == 0) {
        /*  Empty result — emit two NUL bytes if requested, else 0. */
        if (terminate) {
            char *p = (char *)sf_xalloc(a, 2);
            if (!p) { sf_xfree(a, wbuf); return SF_ERR_OOM; }
            p[0] = 0; p[1] = 0;
            *out = p;
            if (out_size_bytes) *out_size_bytes = 2;
        } else {
            char *p = (char *)sf_xalloc(a, 1);
            if (!p) { sf_xfree(a, wbuf); return SF_ERR_OOM; }
            p[0] = 0;
            *out = p;
            if (out_size_bytes) *out_size_bytes = 0;
        }
        sf_xfree(a, wbuf);
        return SF_OK;
    }

    uint8_t *buf = (uint8_t *)sf_xalloc(a, total_bytes);
    if (!buf) { sf_xfree(a, wbuf); return SF_ERR_OOM; }
    /*  Win32 wchar_t IS UTF-16 LE on Windows, so memcpy is sufficient. */
    if (wlen > 0) memcpy(buf, wbuf, (size_t)wlen * 2u);
    if (terminate) {
        buf[(size_t)wlen * 2u]     = 0;
        buf[(size_t)wlen * 2u + 1] = 0;
    }
    sf_xfree(a, wbuf);
    *out = buf;
    if (out_size_bytes) *out_size_bytes = total_bytes;
    return SF_OK;
}

sf_result_t sf_utf8_to_utf16be(const char *utf8, bool terminate,
                               void **out, size_t *out_size_bytes,
                               const sf_allocator_t *a) {
    /*  Convert to LE, then byte-swap each code unit in place. */
    void *le = NULL;
    size_t le_size = 0;
    sf_result_t r = sf_utf8_to_utf16le(utf8, terminate, &le, &le_size, a);
    if (r != SF_OK) return r;

    uint8_t *bytes = (uint8_t *)le;
    /*  Swap pairs. If size is odd (only happens with the empty + !terminate
     *  case where size is 0), nothing to swap. */
    for (size_t i = 0; i + 1 < le_size; i += 2) {
        uint8_t tmp = bytes[i];
        bytes[i]     = bytes[i + 1];
        bytes[i + 1] = tmp;
    }

    *out = le;
    if (out_size_bytes) *out_size_bytes = le_size;
    return SF_OK;
}
