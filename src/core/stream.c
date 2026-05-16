/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_istream_t / sf_ostream_t — read-only and write-only byte streams,
 * backed by either a Win32 file handle or an in-memory growing buffer.
 */

#include "souls_formats/sf_io.h"

#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

/*===========================================================================
 * Backend kinds
 *===========================================================================*/

typedef enum sfi_backend {
    SFI_BK_FILE = 1,
    SFI_BK_MEMORY,
} sfi_backend_t;

/*---------------------------------------------------------------------------
 * Read stream
 *---------------------------------------------------------------------------*/

struct sf_istream {
    sfi_backend_t backend;
    const sf_allocator_t *alloc;

    /*  File backend. */
    HANDLE file;        /* INVALID_HANDLE_VALUE if not used */
    int64_t file_len;
    int64_t file_pos;   /* tracked locally so position() doesn't syscall */

    /*  Memory backend (always borrowed; we never free input data). */
    const uint8_t *mem;
    size_t mem_size;
    int64_t mem_pos;
};

const sf_allocator_t *sfi_istream_allocator(const sf_istream_t *s) {
    return s ? s->alloc : sf_default_allocator();
}

static sf_result_t istream_alloc(sf_istream_t **out, const sf_allocator_t *a,
                                 sfi_backend_t kind) {
    a = sf_alloc_or_default(a);
    sf_istream_t *s = (sf_istream_t *)sf_xalloc(a, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    memset(s, 0, sizeof(*s));
    s->backend = kind;
    s->alloc   = a;
    s->file    = INVALID_HANDLE_VALUE;
    *out = s;
    return SF_OK;
}

sf_result_t sf_istream_open_wfile(sf_istream_t **out, const wchar_t *path,
                                  const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    *out = NULL;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return SF_ERR_IO;

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) {
        CloseHandle(h);
        return SF_ERR_IO;
    }

    sf_istream_t *s = NULL;
    sf_result_t r = istream_alloc(&s, a, SFI_BK_FILE);
    if (r != SF_OK) {
        CloseHandle(h);
        return r;
    }
    s->file     = h;
    s->file_len = (int64_t)sz.QuadPart;
    *out = s;
    return SF_OK;
}

sf_result_t sf_istream_open_file(sf_istream_t **out, const char *utf8_path,
                                 const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(utf8_path != NULL);

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   utf8_path, -1, NULL, 0);
    if (wlen <= 0) return SF_ERR_INVALID_ARG;

    a = sf_alloc_or_default(a);
    wchar_t *wpath = (wchar_t *)sf_xalloc(a, (size_t)wlen * sizeof(wchar_t));
    if (!wpath) return SF_ERR_OOM;

    int got = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  utf8_path, -1, wpath, wlen);
    if (got != wlen) {
        sf_xfree(a, wpath);
        return SF_ERR_INVALID_ARG;
    }

    sf_result_t r = sf_istream_open_wfile(out, wpath, a);
    sf_xfree(a, wpath);
    return r;
}

sf_result_t sf_istream_open_memory(sf_istream_t **out, const void *data, size_t size,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    if (size > 0) SF_CHECK_ARG(data != NULL);

    sf_istream_t *s = NULL;
    sf_result_t r = istream_alloc(&s, a, SFI_BK_MEMORY);
    if (r != SF_OK) return r;
    s->mem      = (const uint8_t *)data;
    s->mem_size = size;
    s->mem_pos  = 0;
    *out = s;
    return SF_OK;
}

void sf_istream_close(sf_istream_t *s) {
    if (!s) return;
    if (s->backend == SFI_BK_FILE && s->file != INVALID_HANDLE_VALUE) {
        CloseHandle(s->file);
    }
    sf_xfree(s->alloc, s);
}

int64_t sf_istream_position(const sf_istream_t *s) {
    if (!s) return 0;
    if (s->backend == SFI_BK_MEMORY) return s->mem_pos;
    return s->file_pos;
}

int64_t sf_istream_length(const sf_istream_t *s) {
    if (!s) return 0;
    if (s->backend == SFI_BK_MEMORY) return (int64_t)s->mem_size;
    return s->file_len;
}

int64_t sf_istream_remaining(const sf_istream_t *s) {
    if (!s) return 0;
    return sf_istream_length(s) - sf_istream_position(s);
}

sf_result_t sf_istream_seek(sf_istream_t *s, int64_t pos) {
    SF_CHECK_ARG(s != NULL);
    SF_CHECK_ARG(pos >= 0);
    if (s->backend == SFI_BK_MEMORY) {
        if (pos > (int64_t)s->mem_size) return SF_ERR_OUT_OF_RANGE;
        s->mem_pos = pos;
        return SF_OK;
    }
    LARGE_INTEGER li;
    li.QuadPart = pos;
    if (!SetFilePointerEx(s->file, li, NULL, FILE_BEGIN)) return SF_ERR_IO;
    s->file_pos = pos;
    return SF_OK;
}

sf_result_t sf_istream_read(sf_istream_t *s, void *buf, size_t n) {
    SF_CHECK_ARG(s != NULL);
    if (n == 0) return SF_OK;
    SF_CHECK_ARG(buf != NULL);

    if (s->backend == SFI_BK_MEMORY) {
        int64_t avail = (int64_t)s->mem_size - s->mem_pos;
        if (avail < 0 || (uint64_t)avail < (uint64_t)n) return SF_ERR_TRUNCATED;
        memcpy(buf, s->mem + s->mem_pos, n);
        s->mem_pos += (int64_t)n;
        return SF_OK;
    }

    /*  Win32 ReadFile reads up to a 32-bit count per call. Loop for >4 GiB. */
    uint8_t *p = (uint8_t *)buf;
    size_t left = n;
    while (left > 0) {
        DWORD chunk = (left > (size_t)0x40000000u) ? 0x40000000u : (DWORD)left;
        DWORD got = 0;
        if (!ReadFile(s->file, p, chunk, &got, NULL)) return SF_ERR_IO;
        if (got == 0) return SF_ERR_TRUNCATED;
        p    += got;
        left -= got;
        s->file_pos += (int64_t)got;
    }
    return SF_OK;
}

/*---------------------------------------------------------------------------
 * Write stream
 *---------------------------------------------------------------------------*/

struct sf_ostream {
    sfi_backend_t backend;
    const sf_allocator_t *alloc;

    /*  File backend. */
    HANDLE file;       /* INVALID_HANDLE_VALUE if not used */
    int64_t file_len;
    int64_t file_pos;  /* tracked locally so position() doesn't syscall */

    /*  Memory backend. */
    uint8_t *mem;
    size_t mem_size;   /* logical written length */
    size_t mem_cap;
    size_t mem_pos;
};

const sf_allocator_t *sfi_ostream_allocator(const sf_ostream_t *s) {
    return s ? s->alloc : sf_default_allocator();
}

sf_result_t sfi_ostream_to_array(const sf_ostream_t *s, const sf_allocator_t *a,
                                 uint8_t **out, size_t *out_size) {
    SF_CHECK_ARG(s != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);

    *out = NULL;
    *out_size = 0;
    size_t size = (s->backend == SFI_BK_MEMORY) ? s->mem_size : (size_t)s->file_len;
    uint8_t *copy = NULL;
    if (size > 0) {
        a = sf_alloc_or_default(a);
        copy = (uint8_t *)sf_xalloc(a, size);
        if (!copy) return SF_ERR_OOM;
    }

    if (s->backend == SFI_BK_MEMORY) {
        if (size > 0) memcpy(copy, s->mem, size);
    } else if (size > 0) {
        LARGE_INTEGER cur;
        cur.QuadPart = s->file_pos;
        LARGE_INTEGER zero = {0};
        if (!SetFilePointerEx(s->file, zero, NULL, FILE_BEGIN)) {
            sf_xfree(a, copy);
            return SF_ERR_IO;
        }
        uint8_t *p = copy;
        size_t left = size;
        while (left > 0) {
            DWORD chunk = (left > (size_t)0x40000000u) ? 0x40000000u : (DWORD)left;
            DWORD got = 0;
            if (!ReadFile(s->file, p, chunk, &got, NULL) || got == 0) {
                SetFilePointerEx(s->file, cur, NULL, FILE_BEGIN);
                sf_xfree(a, copy);
                return SF_ERR_IO;
            }
            p += got;
            left -= got;
        }
        (void)SetFilePointerEx(s->file, cur, NULL, FILE_BEGIN);
    }

    *out = copy;
    *out_size = size;
    return SF_OK;
}

static sf_result_t ostream_alloc(sf_ostream_t **out, const sf_allocator_t *a,
                                 sfi_backend_t kind) {
    a = sf_alloc_or_default(a);
    sf_ostream_t *s = (sf_ostream_t *)sf_xalloc(a, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    memset(s, 0, sizeof(*s));
    s->backend = kind;
    s->alloc   = a;
    s->file    = INVALID_HANDLE_VALUE;
    *out = s;
    return SF_OK;
}

sf_result_t sf_ostream_open_memory(sf_ostream_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    sf_ostream_t *s = NULL;
    sf_result_t r = ostream_alloc(&s, a, SFI_BK_MEMORY);
    if (r != SF_OK) return r;
    *out = s;
    return SF_OK;
}

sf_result_t sf_ostream_open_wfile(sf_ostream_t **out, const wchar_t *path,
                                  const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    *out = NULL;

    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return SF_ERR_IO;

    sf_ostream_t *s = NULL;
    sf_result_t r = ostream_alloc(&s, a, SFI_BK_FILE);
    if (r != SF_OK) {
        CloseHandle(h);
        return r;
    }
    s->file     = h;
    s->file_len = 0;
    s->file_pos = 0;
    *out = s;
    return SF_OK;
}

sf_result_t sf_ostream_open_file(sf_ostream_t **out, const char *utf8_path,
                                 const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(utf8_path != NULL);

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   utf8_path, -1, NULL, 0);
    if (wlen <= 0) return SF_ERR_INVALID_ARG;

    a = sf_alloc_or_default(a);
    wchar_t *wpath = (wchar_t *)sf_xalloc(a, (size_t)wlen * sizeof(wchar_t));
    if (!wpath) return SF_ERR_OOM;

    int got = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  utf8_path, -1, wpath, wlen);
    if (got != wlen) {
        sf_xfree(a, wpath);
        return SF_ERR_INVALID_ARG;
    }
    sf_result_t r = sf_ostream_open_wfile(out, wpath, a);
    sf_xfree(a, wpath);
    return r;
}

void sf_ostream_close(sf_ostream_t *s) {
    if (!s) return;
    if (s->backend == SFI_BK_FILE && s->file != INVALID_HANDLE_VALUE) {
        CloseHandle(s->file);
    }
    if (s->backend == SFI_BK_MEMORY) {
        sf_xfree(s->alloc, s->mem);
    }
    sf_xfree(s->alloc, s);
}

int64_t sf_ostream_position(const sf_ostream_t *s) {
    if (!s) return 0;
    return (s->backend == SFI_BK_MEMORY) ? (int64_t)s->mem_pos : s->file_pos;
}

int64_t sf_ostream_length(const sf_ostream_t *s) {
    if (!s) return 0;
    return (s->backend == SFI_BK_MEMORY) ? (int64_t)s->mem_size : s->file_len;
}

sf_result_t sf_ostream_seek(sf_ostream_t *s, int64_t pos) {
    SF_CHECK_ARG(s != NULL);
    SF_CHECK_ARG(pos >= 0);
    if (s->backend == SFI_BK_MEMORY) {
        if ((size_t)pos > s->mem_size) {
            /*  Seeking past EOF on a write stream — pad to grow. */
            sf_result_t r = sf_ostream_write(s, NULL, 0);
            (void)r;
            /*  Actual grow happens on first write. We just record the new pos
             *  and the next write will fill the gap with zeros. */
        }
        s->mem_pos = (size_t)pos;
        return SF_OK;
    }
    LARGE_INTEGER li;
    li.QuadPart = pos;
    if (!SetFilePointerEx(s->file, li, NULL, FILE_BEGIN)) return SF_ERR_IO;
    s->file_pos = pos;
    return SF_OK;
}

static sf_result_t mem_grow(sf_ostream_t *s, size_t need_total) {
    if (need_total <= s->mem_cap) return SF_OK;
    size_t new_cap = s->mem_cap ? s->mem_cap : 64;
    while (new_cap < need_total) {
        if (new_cap > (SIZE_MAX / 2)) { new_cap = need_total; break; }
        new_cap *= 2;
    }
    uint8_t *p = (uint8_t *)sf_xrealloc(s->alloc, s->mem, s->mem_cap, new_cap);
    if (!p) return SF_ERR_OOM;
    s->mem     = p;
    s->mem_cap = new_cap;
    return SF_OK;
}

sf_result_t sf_ostream_write(sf_ostream_t *s, const void *buf, size_t n) {
    SF_CHECK_ARG(s != NULL);
    if (n == 0) return SF_OK;
    SF_CHECK_ARG(buf != NULL);

    if (s->backend == SFI_BK_MEMORY) {
        size_t end = s->mem_pos + n;
        if (end < n) return SF_ERR_OUT_OF_RANGE;          /* overflow */
        sf_result_t r = mem_grow(s, end);
        if (r != SF_OK) return r;
        /*  If pos > mem_size, zero-fill the gap. */
        if (s->mem_pos > s->mem_size) {
            memset(s->mem + s->mem_size, 0, s->mem_pos - s->mem_size);
        }
        memcpy(s->mem + s->mem_pos, buf, n);
        s->mem_pos = end;
        if (end > s->mem_size) s->mem_size = end;
        return SF_OK;
    }

    const uint8_t *p = (const uint8_t *)buf;
    size_t left = n;
    while (left > 0) {
        DWORD chunk = (left > (size_t)0x40000000u) ? 0x40000000u : (DWORD)left;
        DWORD wrote = 0;
        if (!WriteFile(s->file, p, chunk, &wrote, NULL)) return SF_ERR_IO;
        if (wrote == 0) return SF_ERR_IO;
        p           += wrote;
        left        -= wrote;
        s->file_pos += (int64_t)wrote;
    }
    if (s->file_pos > s->file_len) s->file_len = s->file_pos;
    return SF_OK;
}

sf_result_t sf_ostream_detach_buffer(sf_ostream_t *s, void **out_data, size_t *out_size) {
    SF_CHECK_ARG(s != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    if (s->backend != SFI_BK_MEMORY) return SF_ERR_INVALID_ARG;

    *out_data = s->mem;
    *out_size = s->mem_size;
    s->mem      = NULL;
    s->mem_cap  = 0;
    s->mem_size = 0;
    s->mem_pos  = 0;
    return SF_OK;
}

sf_result_t sf_ostream_reserve(sf_ostream_t *s, size_t capacity) {
    SF_CHECK_ARG(s != NULL);
    if (s->backend != SFI_BK_MEMORY) return SF_ERR_INVALID_ARG;
    if (capacity <= s->mem_cap) return SF_OK;
    return mem_grow(s, capacity);
}
