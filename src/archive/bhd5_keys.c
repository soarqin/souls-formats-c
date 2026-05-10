/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BHD5 archive public RSA keys. Provenance for every key below:
 *
 *   Source: Nordgaren/UXM-Selective-Unpack (GPL-3.0)
 *           https://github.com/Nordgaren/UXM-Selective-Unpack
 *           UXM/ArchiveKeys.cs @ master
 *
 * Each key was extracted verbatim (PKCS#1 RSAPublicKey PEM) from that
 * file and pasted below. T0d only ships the entry-point key per game;
 * additional Data1..N / DLC / sd keys are added in T10.
 *
 *   SF_BHD5_GAME_SEKIRO       → SekiroKeys["Data1"]
 *       (Sekiro has no Data0; Data1 is the first/main archive header)
 *   SF_BHD5_GAME_ELDENRING    → EldenRingKeys["Data0"]
 *   SF_BHD5_GAME_NIGHTREIGN   → EldenRingNightreignKeys["Data0"]
 *   SF_BHD5_GAME_ARMOREDCORE6 → ArmoredCore6Keys["Data0"]
 */

#include "archive/bhd5_keys.h"

static const char SF_BHD5_PEM_KEY_SEKIRO[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA92l+AWx1aV7mzt+6r00bm/qnc4b6NH3VVr/v4UxMcfzushL8jsn9\n"
    "ZSP1ss95ot/quk8dOJsp0+/bvxH+C9DEezzNLSqqAGd2jq2PYosj/6FhYAKjjMlK\n"
    "jNxcVPsKQug0Zby+KYsENirmEXcmA1fzltrISf6d6LKB1UFHHN9NRkLCm3idE4Pu\n"
    "9852kPHbiL14EqfDCDgwm7kLeQdt3kUbcmdhu/6dvP42HGxBmAYLNFD3iAe7qLML\n"
    "MFzmKKHQD2fRQK/431Z3xPK6Jp245AdR0AwUYVvnXq+/97wMX0C6UKvAZ+b/1ytD\n"
    "Nu8vZt++lhJ01SjTc2A4hVPz7g1EEO5/TQIEKkj5Jw==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_ELDENRING[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA9Rju2whruXDVQZpfylVEPeNxm7XgMHcDyaaRUIpXQE0qEo+6Y36L\n"
    "P0xpFvL0H0kKxHwpuISsdgrnMHJ/yj4S61MWzhO8y4BQbw/zJehhDSRCecFJmFBz\n"
    "3I2JC5FCjoK+82xd9xM5XXdfsdBzRiSghuIHL4qk2WZ/0f/nK5VygeWXn/oLeYBL\n"
    "jX1S8wSSASza64JXjt0bP/i6mpV2SLZqKRxo7x2bIQrR1yHNekSF2jBhZIgcbtMB\n"
    "xjCywn+7p954wjcfjxB5VWaZ4hGbKhi1bhYPccht4XnGhcUTWO3NmJWslwccjQ4k\n"
    "sutLq3uRjLMM0IeTkQO6Pv8/R7UNFtdCWwIERzH8IQ==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_NIGHTREIGN[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAz8F9U1V9hgKs40gdzl1ZOf3IBirf6xUEzXtDd6oSEBE6XiYocvAB\n"
    "ykiK+WMdAaJL7HJ58Gt2xSRxA3t9toCGKMI/3gNAfcR0BV83gsQo0O0dVP0fqyxX\n"
    "lA2pGN5B4IE8aLWPX2cNNFSFKAdjYnzsYSevzef/pgnpV1ZgPf2j2SQwNGSufYeN\n"
    "3Owji8l0K2C0fKIx6gSO0cK9kvTIm8AdpvzZbBkTylT1jF3m8DsSA1OFzFJTdFyZ\n"
    "bTRi85M6bmv6rHtvZc5OW21dye7Q6fmLlxOyMetLTu4dpOXjHAAf/LFTbfQpXFr9\n"
    "aXO4O6I7nWDJn7FRzNlLkb8RwSyZ1/KWyQIFALEDsAc=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_ARMOREDCORE6[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEA7F43Ss9kroawBSUW6GBhSUo6GtxYtUV8zCPkcHhSJLGPASHhwsaX\n"
    "zMRrd+Ul9qB3oYchb4xYtdMWKFe0/ZDi9vgYXvF3rlWaZKAu1k/F6RwVAd//I3Kj\n"
    "JsYlhayskInKqB3BvB/KL2Ga8QBsZ/G9cLlUYsqIj3as9oqbfEXVmGVeuhg0I+NQ\n"
    "NL+2sThqp5eOQstfXQgqduOt0ixd/r9e5VjLhyj2z4hCEF2TVsDw9wGEBem1TkcO\n"
    "C/E8obl9fTHwEK7l2i8a4HafY7flU220r8y4UwQ+9Aq94xUYT2xdcTjdyBIaZtyS\n"
    "YmR86B680OyL9oiEonEFhh4cor/84PSmNQIFAOHX27k=\n"
    "-----END RSA PUBLIC KEY-----\n";

const char *sfi_bhd5_get_pem_key(sf_bhd5_game_t game) {
    switch (game) {
    case SF_BHD5_GAME_SEKIRO:       return SF_BHD5_PEM_KEY_SEKIRO;
    case SF_BHD5_GAME_ELDENRING:    return SF_BHD5_PEM_KEY_ELDENRING;
    case SF_BHD5_GAME_NIGHTREIGN:   return SF_BHD5_PEM_KEY_NIGHTREIGN;
    case SF_BHD5_GAME_ARMOREDCORE6: return SF_BHD5_PEM_KEY_ARMOREDCORE6;
    case SF_BHD5_GAME_COUNT_:
    default:                        return NULL;
    }
}
