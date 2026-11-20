/*
 * Unbiased bounded MT19937 draws and period-class Fisher-Yates shuffle.
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "calendar_internal.h"

/* Let M=2^32 and t=M mod b. Reject [0,t), then the remaining
 * M-t integers have equal-sized residue classes modulo b. */
uint32_t calendar_random_bounded(mtwister_t *rng, uint32_t bound) {
    if (bound <= 1U) return 0U;
    const uint32_t threshold = (uint32_t)(0U - bound) % bound;
    uint32_t x;
    do { x = mtwister_next_u32(rng); } while (x < threshold);
    return x % bound;
}

void calendar_shuffle_group(mtwister_t *rng, fixture_t *base, size_t first,
                            size_t length, size_t k) {
    for (size_t i = length; i > 1U; --i) {
        size_t j = calendar_random_bounded(rng, (uint32_t)i);
        size_t left=(first+i-1U)*k,right=(first+j)*k;
        for (size_t f=0;f<k;++f) {
            fixture_t temporary=base[left+f];
            base[left+f]=base[right+f];
            base[right+f]=temporary;
        }
    }
}

void calendar_shuffle_base(const calendar_generator_t *g,
                           mtwister_t *rng, fixture_t *base) {
    const size_t split=g->frequency_group_split;
    calendar_shuffle_group(rng,base,0U,split,g->fixtures_per_round);
    calendar_shuffle_group(rng,base,split,g->base_rounds-split,
                           g->fixtures_per_round);
}
