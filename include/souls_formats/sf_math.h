/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — POD math types.
 *
 * The library does I/O, not math. These structs only define memory layout
 * compatible with FromSoftware files; consumers can copy fields into their
 * preferred math library (cglm, HandmadeMath, DirectXMath, etc.) for
 * computation.
 *
 * Memory layout is plain `float` fields in declaration order, equivalent to
 * `System.Numerics.Vector{2,3,4}` / `Quaternion` / `Matrix4x4` in C#.
 */

#ifndef SOULS_FORMATS_SF_MATH_H
#define SOULS_FORMATS_SF_MATH_H

#if __has_include("souls_formats/sf_common.h")
#include "souls_formats/sf_common.h"
#else
#include "sf_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_vec2 {
    float x, y;
} sf_vec2_t;

typedef struct sf_vec3 {
    float x, y, z;
} sf_vec3_t;

typedef struct sf_vec4 {
    float x, y, z, w;
} sf_vec4_t;

/*  XYZW-ordered, matching Microsoft's System.Numerics.Quaternion. */
typedef struct sf_quat {
    float x, y, z, w;
} sf_quat_t;

/*  Row-major 4x4, matching System.Numerics.Matrix4x4 layout (m11..m44). */
typedef struct sf_mat4 {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
} sf_mat4_t;

/*  ARGB color, one byte per channel. Layout matches System.Drawing.Color
 *  field order used by upstream (A, R, G, B). */
typedef struct sf_color {
    uint8_t a, r, g, b;
} sf_color_t;

_Static_assert(sizeof(sf_vec2_t)  ==  8, "sf_vec2_t layout mismatch");
_Static_assert(sizeof(sf_vec3_t)  == 12, "sf_vec3_t layout mismatch");
_Static_assert(sizeof(sf_vec4_t)  == 16, "sf_vec4_t layout mismatch");
_Static_assert(sizeof(sf_quat_t)  == 16, "sf_quat_t layout mismatch");
_Static_assert(sizeof(sf_mat4_t)  == 64, "sf_mat4_t layout mismatch");
_Static_assert(sizeof(sf_color_t) ==  4, "sf_color_t layout mismatch");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MATH_H */
