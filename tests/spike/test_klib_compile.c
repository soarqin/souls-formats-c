/* T0.1 klib compile spike — remove after Wave 4 (T4.1) */
#include "khash.h"
#include <stdio.h>

KHASH_MAP_INIT_INT64(bhd5_map, void *)

int main(void) {
    khash_t(bhd5_map) *h = kh_init(bhd5_map);
    int ret;
    khiter_t k = kh_put(bhd5_map, h, 12345678901LL, &ret);
    kh_value(h, k) = (void *)0xDEAD;
    k = kh_get(bhd5_map, h, 12345678901LL);
    if (k != kh_end(h) && kh_value(h, k) == (void *)0xDEAD) {
        printf("KLIB-SPIKE OK\n");
    }
    kh_destroy(bhd5_map, h);
    return 0;
}
