/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BHD5 archive public RSA keys. Provenance for every key below:
 *
 *   Source: Nordgaren/UXM-Selective-Unpack (GPL-3.0)
 *           https://github.com/Nordgaren/UXM-Selective-Unpack
 *           UXM/ArchiveKeys.cs @ master
 *
 * Each key was extracted verbatim (PKCS#1 RSAPublicKey PEM) from that
 * file and pasted below. Sekiro signs each Data shard (Data1..Data5)
 * with a distinct private key, so the shard array below covers all
 * five; sfi_bhd5_get_pem_key() returns the Data1 entry-point key as
 * the default.
 *
 *   SF_BHD5_GAME_SEKIRO       → SekiroKeys["Data1".."Data5"]
 *       (Sekiro has no Data0; Data1 is the first/main archive header)
 *   SF_BHD5_GAME_ELDENRING    → EldenRingKeys["Data0"]
 *   SF_BHD5_GAME_NIGHTREIGN   → EldenRingNightreignKeys["Data0"]
 *   SF_BHD5_GAME_ARMOREDCORE6 → ArmoredCore6Keys["Data0"]
 *   SF_BHD5_GAME_DARKSOULS3   → DarkSouls3Keys["Data1"]
 */

#include "archive/bhd5_keys.h"

static const char SF_BHD5_PEM_KEY_SEKIRO_DATA1[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA92l+AWx1aV7mzt+6r00bm/qnc4b6NH3VVr/v4UxMcfzushL8jsn9\n"
    "ZSP1ss95ot/quk8dOJsp0+/bvxH+C9DEezzNLSqqAGd2jq2PYosj/6FhYAKjjMlK\n"
    "jNxcVPsKQug0Zby+KYsENirmEXcmA1fzltrISf6d6LKB1UFHHN9NRkLCm3idE4Pu\n"
    "9852kPHbiL14EqfDCDgwm7kLeQdt3kUbcmdhu/6dvP42HGxBmAYLNFD3iAe7qLML\n"
    "MFzmKKHQD2fRQK/431Z3xPK6Jp245AdR0AwUYVvnXq+/97wMX0C6UKvAZ+b/1ytD\n"
    "Nu8vZt++lhJ01SjTc2A4hVPz7g1EEO5/TQIEKkj5Jw==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_SEKIRO_DATA2[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAqhjoThWX8VwsTKTI1kjp0JBloCXhV8i99P1KPTCTDBnmhVQPdu+7\n"
    "UQ5g4//eh0oqKaOUjet+0SP94QscjIIrhV91OzfIouIWgJJK/ROOP/A3sb5AlzPa\n"
    "6YPcN8ODxR+esyrWhc6rHCt4qGvXVXrgh6zpZM5h5VCTSaup4qqIWm44EF3+FeYS\n"
    "7faFg14rH0QEosieIIZFZmpI6SCJanlrVd+Zh13s4XcZfk0JdC2AEjxCQ2lKi3Un\n"
    "WAMOcJc+8uHoMuNNo1PMpYQ6Z8Nzg5Cii7EnwbCDmuJw58tFBmbOVHZpkY93VIeF\n"
    "maJXSE7ztTp0qTa05YZUsiU3g9HplkeTUwIFAP/xKZE=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_SEKIRO_DATA3[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAx5jlgIvoHQLwSFsAwKFZbNo3fgZ89C7tj4hwiZsQVg8QnNZohXl5\n"
    "S5Ep9pS2biOFsSkuZMXKmfYErh2CsdFbr7QR7kvPPianXNrkCI4xlfQwJvMmkLm9\n"
    "6/JmRIUzTWp0kKJUJZJH/UIrXNn7fmk8Vmx1bQIi8bumGSl3gxeMhutv/lC9khsY\n"
    "Tn0ABTJAbIbwNZ5GPXxzQZuQPXXDY52Gm+Fx7Yy1LiK/B6isIDJUN0xdgxdaXxGN\n"
    "f5pPocMJjng0Ob3cjhGvdkysll/jYFnRx0La3CGmtLcXMtHheEQxzGueGDa/lkkl\n"
    "AvvEXtcpKfyFQWcUheQZ8LngAh/UTJHtQwIFAOpVoU8=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_SEKIRO_DATA4[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAq8RyArk+eqMAcxLAHUDRYV7yScNKZpKSxGmgJZQ7y6Y8f5wdrNCt\n"
    "byXfmsdQECStIGlkwWjtfm8t/bRZuxxPciAYaFsWo0Ze2BB6uY6ZteNpLJn82qbL\n"
    "TXATf+af3kSrvICfvJwRzbfA/PRJRkHj2gJ6Tc7g6HK7S/4TiCZirq+c/zLY3gb8\n"
    "A8uIFNI4j0qxTzfoAlS7K6spZjfnhZ6l7pYFh+glz15wAbppC9Oy/u5vUacozf4v\n"
    "nacbUHD47ds9EZPZDHk3LfJbioHwtUzJfyBqZmIpI33yiwImPpb96zwvQU86TaXK\n"
    "sJrTmSs/48BeDsQwXuaqOg+6noETBx3pgQIEGM2Ohw==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char SF_BHD5_PEM_KEY_SEKIRO_DATA5[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAu75/UbXwHdvu/p49TwnY7Ou6DAuZYFAtLUkw/R4nvm0HWVlRsZiB\n"
    "LG3MOG6sPmK2Zc3JLBU2QK4uKazZ9VrmotM4OpYr03q2tiFnv3NfCvB1UeIJIKe3\n"
    "kVhHNZIbvrwEP9a5UCnrSHD+u+Fj5MQBr4yrEitwrNVvIC4J0Ez1Ppn3+D8ff8Xg\n"
    "QRP9qCVLI3X/wdQDea+B5o8PWaYEL9MKnnL1Tq4h+4PRYHcQR8/GXBTrc3x9q3cP\n"
    "QRDWHbRYhIfWSP9urtagjcsmcuG+p34fp+KyWOwkil3FJqwH1KgSTbk9Tb0oBPzq\n"
    "TCJKeE/wgu6hY++lBi5T3ArHZZcsbXzV6wIFAPlRTMc=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char *const SF_BHD5_PEM_KEYS_SEKIRO[5] = {
    SF_BHD5_PEM_KEY_SEKIRO_DATA1,
    SF_BHD5_PEM_KEY_SEKIRO_DATA2,
    SF_BHD5_PEM_KEY_SEKIRO_DATA3,
    SF_BHD5_PEM_KEY_SEKIRO_DATA4,
    SF_BHD5_PEM_KEY_SEKIRO_DATA5,
};

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

static const char SF_BHD5_PEM_KEY_DARKSOULS3[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA05hqyboW/qZaJ3GBIABFVt1X1aa0/sKINklvpkTRC+5Ytbxvp18L\n"
    "M1gN6gjTgSJiPUgdlaMbptVa66MzvilEk60aHyVVEhtFWy+HzUZ3xRQm6r/2qsK3\n"
    "8wXndgEU5JIT2jrBXZcZfYDCkUkjsGVkYqjBNKfp+c5jlnNwbieUihWTSEO+DA8n\n"
    "aaCCzZD3e7rKhDQyLCkpdsGmuqBvl02Ou7QeehbPPno78mOYs2XkP6NGqbFFGQwa\n"
    "swyyyXlQ23N15ZaFGRRR0xYjrX4LSe6OJ8Mx/Zkec0o7L28CgwCTmcD2wO8TEATE\n"
    "AUbbV+1Su9uq2+wQxgnsAp+xzhn9og9hmwIEC35bSQ==\n"
    "-----END RSA PUBLIC KEY-----\n";

const char *sfi_bhd5_get_pem_key(sf_bhd5_game_t game) {
    switch (game) {
    case SF_BHD5_GAME_SEKIRO:       return SF_BHD5_PEM_KEY_SEKIRO_DATA1;
    case SF_BHD5_GAME_ELDENRING:    return SF_BHD5_PEM_KEY_ELDENRING;
    case SF_BHD5_GAME_NIGHTREIGN:   return SF_BHD5_PEM_KEY_NIGHTREIGN;
    case SF_BHD5_GAME_ARMOREDCORE6: return SF_BHD5_PEM_KEY_ARMOREDCORE6;
    case SF_BHD5_GAME_DARKSOULS3:   return SF_BHD5_PEM_KEY_DARKSOULS3;
    case SF_BHD5_GAME_COUNT_:
    default:                        return NULL;
    }
}

const char *sfi_bhd5_get_sekiro_shard_key(int shard) {
    if (shard < 1 || shard > 5) return NULL;
    return SF_BHD5_PEM_KEYS_SEKIRO[shard - 1];
}
