/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — common types, error codes, allocator interface.
 * This header is the foundation of every other public header in the library.
 */

#ifndef SOULS_FORMATS_SF_COMMON_H
#define SOULS_FORMATS_SF_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Version
 *===========================================================================*/

#define SF_VERSION_MAJOR 0
#define SF_VERSION_MINOR 6
#define SF_VERSION_PATCH 0
#define SF_VERSION_STRING "0.6.0"

/*===========================================================================
 * ABI export macro
 *
 *  - When building the shared library:   SF_BUILD_SHARED + SF_BUILD_DLL → export
 *  - When consuming the shared library:  SF_BUILD_SHARED only          → import
 *  - When building/consuming static:     neither defined               → no-op
 *
 * See PLAN.md §5.6 for rationale.
 *===========================================================================*/
#if defined(_WIN32) && defined(SF_BUILD_SHARED)
#  if defined(SF_BUILD_DLL)
#    define SF_API __declspec(dllexport)
#  else
#    define SF_API __declspec(dllimport)
#  endif
#else
#  define SF_API
#endif

/*===========================================================================
 * Result codes
 *
 * Every API that may fail returns sf_result_t. Output is via pointer
 * parameters. SF_OK is always 0; non-zero means failure.
 *===========================================================================*/
typedef enum sf_result {
    SF_OK = 0,
    SF_ERR_INVALID_ARG,
    SF_ERR_OOM,
    SF_ERR_IO,
    SF_ERR_BAD_MAGIC,
    SF_ERR_UNSUPPORTED_VERSION,
    SF_ERR_TRUNCATED,
    SF_ERR_OUT_OF_RANGE,
    SF_ERR_DECOMPRESS,
    SF_ERR_OODLE_NOT_FOUND,
    SF_ERR_CRYPTO,
    SF_ERR_NOT_FOUND,
    SF_ERR_ALREADY_EXISTS,
    SF_ERR_INVALID_STATE,
    SF_ERR_UNSUPPORTED,
    SF_ERR_INTERNAL,
    /* Sentinel — must remain the last entry. */
    SF_RESULT_COUNT_
} sf_result_t;

/*  Returns a static, non-null, NUL-terminated description of the result.
 *  Unknown values return "(unknown sf_result_t)". Never returns NULL. */
SF_API const char *sf_result_str(sf_result_t r);

/*  Returns a thread-local optional textual detail of the most recent error
 *  raised on the current thread. May return NULL if none has been recorded.
 *  Strings are valid until the next sf_* call on this thread. */
SF_API const char *sf_last_error_detail(void);

/*===========================================================================
 * Allocator interface
 *
 * All "create" APIs accept a `const sf_allocator_t *alloc` parameter. NULL
 * means use a default malloc/free-backed allocator. An object created with
 * a given allocator must be destroyed by an API that uses the same allocator
 * (which the object remembers internally).
 *===========================================================================*/
typedef struct sf_allocator {
    void *(*alloc)(size_t size, void *user);
    void *(*realloc)(void *p, size_t old_size, size_t new_size, void *user);
    void  (*free)(void *p, void *user);
    void  *user;
} sf_allocator_t;

/*  Returns a singleton default allocator backed by malloc / realloc / free.
 *  Never returns NULL. The returned pointer is valid for the entire program
 *  lifetime. */
SF_API const sf_allocator_t *sf_default_allocator(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_COMMON_H */
