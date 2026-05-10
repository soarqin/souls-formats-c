/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — single umbrella header.
 *
 * Including this pulls in the entire public API of the library. For finer
 * compile-time control, include the individual format headers directly:
 *
 *     #include "souls_formats/sf_dcx.h"
 *     #include "souls_formats/sf_bnd4.h"
 *     ...
 */

#ifndef SOULS_FORMATS_H
#define SOULS_FORMATS_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_math.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_path.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_oodle.h"
#include "souls_formats/sf_regulation.h"
#include "souls_formats/sf_sl2.h"
#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd3.h"
#include "souls_formats/sf_bnd4.h"

/* Format headers will be added here as each Phase lands.
 * Phase 1: io, math, encoding, hash  (DONE)
 * Phase 2: dcx
 * Phase 3: bnd*, bxf*, bhd5, tpf, enfl
 * Phase 4: param, paramdef, paramtdf, fmg
 * Phase 5: emevd, esd, msb*
 * Phase 6: flver*, mtd, matbin
 */

#endif /* SOULS_FORMATS_H */
