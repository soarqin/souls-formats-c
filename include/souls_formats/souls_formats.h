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

#include "sf_common.h"
#include "sf_math.h"
#include "sf_io.h"
#include "sf_encoding.h"
#include "sf_hash.h"
#include "sf_path.h"
#include "sf_dcx.h"
#include "sf_oodle.h"
#include "sf_regulation.h"
#include "sf_sl2.h"
#include "sf_bhd5.h"
#include "sf_binder.h"
#include "sf_bnd.h"
#include "sf_bnd2.h"
#include "sf_bnd3.h"
#include "sf_bnd4.h"
#include "sf_bxf3.h"
#include "sf_bxf4.h"
#include "sf_enfl.h"
#include "sf_tpf.h"
#include "sf_paramtdf.h"
#include "sf_flver.h"
#include "sf_flver0.h"
#include "sf_flver2.h"
#include "sf_mdl4.h"
#include "sf_mdl.h"
#include "sf_mtd.h"
#include "sf_matbin.h"
#include "sf_tae.h"
#include "sf_tae_template.h"
#include "sf_fxr3.h"
#include "sf_btab.h"
#include "sf_btl.h"
#include "sf_gparam.h"
#include "sf_pmdcl.h"
#include "sf_msb1.h"
#include "sf_msb2.h"
#include "sf_msb3.h"
#include "sf_msbac4.h"
#include "sf_msbb.h"
#include "sf_msbd.h"
#include "sf_msbdr.h"
#include "sf_msbfa.h"
#include "sf_msbn.h"
#include "sf_msbv.h"
#include "sf_msbvd.h"
#include "sf_aip.h"
#include "sf_f2tr.h"
#include "sf_clm2.h"
#include "sf_rmb.h"
#include "sf_grass.h"
#include "sf_ccm.h"
#include "sf_edd.h"
#include "sf_acb.h"
#include "sf_smd4.h"
#include "sf_drb.h"
#include "sf_kf4.h"
#include "sf_kuon.h"
#include "sf_mwc.h"
#include "sf_legacy_misc.h"

#include "sf_luagnl.h"
#include "sf_luainfo.h"
#include "sf_emeld.h"
#include "sf_fmb.h"
#include "sf_nmb.h"
#include "sf_nsa.h"

/* Format headers will be added here as each Phase lands.
 * Phase 1: io, math, encoding, hash  (DONE)
 * Phase 2: dcx
 * Phase 3: bnd*, bxf*, bhd5, tpf, enfl
 * Phase 4: param, paramdef, paramtdf, fmg
 * Phase 5: emevd, esd, msb*
 * Phase 6: flver*, mtd, matbin
 * T6.7: luagnl, luainfo, emeld, fmb
 */

#endif /* SOULS_FORMATS_H */
