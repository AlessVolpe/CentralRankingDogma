/*
 * Periodic fixture calendars and independent validation (3.0.12).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "calendar.h"
#include "calendar_internal.h"
#include "alloc_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int multiply(size_t a, size_t b, size_t *out) {
    if (a != 0U && b > SIZE_MAX / a) return -EOVERFLOW;
    *out = a * b;
    return 0;
}

static void *copy_buffer(const void *src, size_t count, size_t element_size) {
    size_t bytes;
    if (multiply(count, element_size, &bytes) != 0) return NULL;
    void *p = leaderboard_alloc_malloc(bytes);
    if (p != NULL) memcpy(p, src, bytes);
    return p;
}

static int same_pair(fixture_t a, fixture_t b) {
    return (a.home == b.home && a.away == b.away) ||
           (a.home == b.away && a.away == b.home);
}

static int same_matching(const fixture_t *a, const fixture_t *b, size_t k) {
    for (size_t i = 0; i < k; ++i) {
        bool found = false;
        for (size_t j = 0; j < k; ++j)
            if (same_pair(a[i], b[j])) { found = true; break; }
        if (!found) return 0;
    }
    return 1;
}

calendar_options_t calendar_options_default(void) {
    calendar_options_t options = {
        CALENDAR_METHOD_RECOMBINE, CALENDAR_SAMPLING_INDEPENDENT,
        UINT32_C(5489), 32U, true, 4U, 4000U, 0U, 0U
    };
    return options;
}

void calendar_destroy(calendar_t *c) {
    if (c == NULL) return;
    free(c->matchdays);
    free(c->fixtures);
    free(c);
}

size_t calendar_round_count(const calendar_t *c) { return c ? c->round_count : 0U; }
size_t calendar_team_count(const calendar_t *c) { return c ? c->team_count : 0U; }

int calendar_round_matchday(const calendar_t *c, size_t r, uint32_t *out) {
    if (c == NULL || out == NULL) return -EINVAL;
    if (r >= c->round_count) return -ERANGE;
    *out = c->matchdays[r];
    return 0;
}
int calendar_fixture_at(const calendar_t *c, size_t r, size_t f, fixture_t *out) {
    if (c == NULL || out == NULL) return -EINVAL;
    if (r >= c->round_count || f >= c->team_count / 2U) return -ERANGE;
    *out = c->fixtures[r * (c->team_count / 2U) + f];
    return 0;
}

/* Canonical fingerprint: sorted partner of every team for every round. This
 * ignores both fixture ordering and home/away direction. */
static uint64_t hash_word(uint64_t h, uint32_t v) {
    for (unsigned k = 0; k < 4U; ++k) {
        h ^= (uint8_t)(v >> (8U * k));
        h *= UINT64_C(1099511628211);
    }
    return h;
}
uint64_t calendar_fingerprint(const calendar_t *c) {
    if (c == NULL) return 0U;
    uint64_t h = UINT64_C(14695981039346656037);
    h = hash_word(h, (uint32_t)c->team_count);
    h = hash_word(h, (uint32_t)c->round_count);
    const size_t k = c->team_count / 2U;
    for (size_t r = 0; r < c->round_count; ++r) {
        h = hash_word(h, c->matchdays[r]);
        for (size_t team = 0; team < c->team_count; ++team) {
            uint32_t opponent = UINT32_MAX;
            for (size_t f = 0; f < k; ++f) {
                fixture_t m = c->fixtures[r*k + f];
                if (m.home == team) opponent = m.away;
                if (m.away == team) opponent = m.home;
            }
            h = hash_word(h, opponent);
        }
    }
    return h;
}

/* Validation must not simply trust internal generator invariants. */
int calendar_validate(const competition_t *source, const calendar_t *c) {
    if (source == NULL || c == NULL) return -EINVAL;
    const size_t n = competition_team_count(source);
    const size_t rcount = competition_round_count(source);
    if (n < 2U || (n & 1U) || c->team_count != n ||
        c->round_count != rcount || c->fixtures == NULL || c->matchdays == NULL)
        return -EINVAL;
    const size_t period = n - 1U;
    size_t square, bytes;
    if (multiply(n, n, &square) != 0 ||
        multiply(square, sizeof(uint32_t), &bytes) != 0) return -EOVERFLOW;
    uint32_t *directed = leaderboard_alloc_calloc(square, sizeof(*directed));
    uint8_t *seen = leaderboard_alloc_calloc(n, sizeof(*seen));
    const size_t base_rounds = rcount < period ? rcount : period;
    size_t partner_count;
    if (multiply(base_rounds, n, &partner_count) != 0 ||
        partner_count > SIZE_MAX / sizeof(team_index_t)) {
        free(directed); free(seen); return -EOVERFLOW;
    }
    team_index_t *reference = leaderboard_alloc_malloc(partner_count * sizeof(*reference));
    if (directed == NULL || seen == NULL || reference == NULL) {
        free(directed); free(seen); free(reference); return -ENOMEM;
    }
    int result = 0;
    const size_t k = n / 2U;
    for (size_t r = 0; r < rcount; ++r) {
        uint32_t mapped = 0U;
        if (competition_round_matchday(source, r, &mapped) != 0 ||
            mapped != c->matchdays[r]) { result = -EINVAL; break; }
        memset(seen, 0, n);
        for (size_t f = 0; f < k; ++f) {
            fixture_t m = c->fixtures[r*k+f];
            if (m.home >= n || m.away >= n || m.home == m.away ||
                seen[m.home] || seen[m.away]) { result = -EINVAL; break; }
            seen[m.home] = seen[m.away] = 1U;
            if (r < period) {
                if (directed[(size_t)m.home*n + m.away] != 0U ||
                    directed[(size_t)m.away*n + m.home] != 0U) {
                    result = -EINVAL; break;
                }
                reference[r*n+m.home]=m.away;
                reference[r*n+m.away]=m.home;
            } else if (reference[(r%period)*n + m.home] != m.away) {
                result = -EINVAL; break;
            }
            if (directed[(size_t)m.home*n + m.away] == UINT32_MAX) {
                result = -EOVERFLOW; break;
            }
            directed[(size_t)m.home*n + m.away]++;
        }
        if (result != 0) break;
    }
    if (result == 0) {
        for (size_t r = 0; r < rcount; ++r) {
            for (size_t f = 0; f < k; ++f) {
                fixture_t m;
                if (competition_fixture_at(source, r, f, &m) != 0) {
                    result = -EINVAL; break;
                }
                size_t offset = (size_t)m.home*n + m.away;
                if (directed[offset] == 0U) { result = -EINVAL; break; }
                directed[offset]--;
            }
            if (result != 0) break;
        }
    }
    if (result == 0) {
        for (size_t i = 0; i < square; ++i)
            if (directed[i] != 0U) { result = -EINVAL; break; }
    }
    free(directed); free(seen); free(reference);
    return result;
}

void calendar_generator_destroy(calendar_generator_t *g) {
    if (g == NULL) return;
    free(g->initial_base);
    free(g->chain_base);
    free(g->orientation);
    free(g);
}

int calendar_generator_create(const competition_t *source,
                              const calendar_options_t *given,
                              calendar_generator_t **out) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (source == NULL) return -EINVAL;
    calendar_options_t options = given ? *given : calendar_options_default();
    if ((options.method != CALENDAR_METHOD_ROUND_SHUFFLE &&
         options.method != CALENDAR_METHOD_RECOMBINE) ||
        (options.sampling_mode != CALENDAR_SAMPLING_INDEPENDENT &&
         options.sampling_mode != CALENDAR_SAMPLING_CHAIN) ||
        (options.enable_three_round &&
         (options.three_round_attempts == 0U ||
          options.three_round_node_limit == 0U))) return -EINVAL;
    const size_t n = competition_team_count(source), r = competition_round_count(source);
    if (n < 2U || (n & 1U) != 0U || r == 0U) return -EINVAL;
    if (n > UINT32_MAX || r > UINT32_MAX) return -EOVERFLOW;
    const size_t p = n-1U, b = r < p ? r : p, k = n/2U;
    size_t base_fixtures, square, orientations, stride = r/p + (r%p != 0U);
    size_t partner_cells;
    if (multiply(b,k,&base_fixtures) != 0 ||
        multiply(b,n,&partner_cells) != 0 ||
        partner_cells > SIZE_MAX / sizeof(uint32_t) ||
        multiply(n,n,&square) != 0 ||
        multiply(square,stride,&orientations) != 0 ||
        base_fixtures > SIZE_MAX / sizeof(fixture_t)) return -EOVERFLOW;
    calendar_generator_t *g = leaderboard_alloc_calloc(1U, sizeof(*g));
    if (g == NULL) return -ENOMEM;
    g->source=source; g->options=options;g->n=n;g->rounds=r;
    g->period=p;g->base_rounds=b;g->fixtures_per_round=k;
    g->frequency_group_split = r % p == 0U ? b : r % p;
    g->orientation_stride=stride;
    g->initial_base=leaderboard_alloc_malloc(base_fixtures*sizeof(fixture_t));
    g->chain_base=leaderboard_alloc_malloc(base_fixtures*sizeof(fixture_t));
    g->orientation=leaderboard_alloc_calloc(orientations,sizeof(*g->orientation));
    if (g->initial_base == NULL || g->chain_base == NULL || g->orientation == NULL) {
        calendar_generator_destroy(g); return -ENOMEM;
    }
    int status = 0;
    uint32_t *base_partner = leaderboard_alloc_malloc(partner_cells*sizeof(*base_partner));
    if (base_partner == NULL) { calendar_generator_destroy(g); return -ENOMEM; }
    for (size_t i = 0; i < partner_cells; ++i) base_partner[i] = UINT32_MAX;
    for (size_t ri = 0; ri < r; ++ri) {
        for (size_t f = 0; f < k; ++f) {
            fixture_t m;
            if (competition_fixture_at(source,ri,f,&m) != 0 ||
                m.home >= n || m.away >= n || m.home == m.away) {
                status = -EINVAL; goto invalid;
            }
            if (ri < b) {
                g->initial_base[ri*k+f]=m;
                if (base_partner[ri*n+m.home] != UINT32_MAX ||
                    base_partner[ri*n+m.away] != UINT32_MAX) {
                    status = -ENOTSUP; goto invalid;
                }
                base_partner[ri*n+m.home]=m.away;
                base_partner[ri*n+m.away]=m.home;
                /* All base matchings must be pairwise edge-disjoint. */
                for (size_t prev=0;prev<ri;++prev)
                    if (base_partner[prev*n+m.home] == m.away) {
                        status = -ENOTSUP;goto invalid;
                    }
            } else if (base_partner[(ri%p)*n+m.home] != m.away) {
                status = -ENOTSUP;goto invalid;
            }
            size_t low = m.home < m.away ? m.home : m.away;
            size_t high = m.home < m.away ? m.away : m.home;
            g->orientation[(low*n+high)*stride+ri/p] = (uint8_t)(m.home == low);
        }
    }
    memcpy(g->chain_base,g->initial_base,base_fixtures*sizeof(fixture_t));
    mtwister_seed_standard(&g->rng,options.seed);
invalid:
    free(base_partner);
    if (status != 0) { calendar_generator_destroy(g); return status; }
    *out=g;return 0;
}

static int materialize(const calendar_generator_t *g, const fixture_t *base,
                       calendar_t **out) {
    calendar_t *c = leaderboard_alloc_calloc(1U,sizeof(*c));
    if (c == NULL) return -ENOMEM;
    c->team_count=g->n; c->round_count=g->rounds;
    size_t count=g->rounds*g->fixtures_per_round;
    c->matchdays=leaderboard_alloc_malloc(g->rounds*sizeof(*c->matchdays));
    c->fixtures=leaderboard_alloc_malloc(count*sizeof(*c->fixtures));
    if (c->matchdays==NULL || c->fixtures==NULL) { calendar_destroy(c);return -ENOMEM; }
    for (size_t r=0;r<g->rounds;++r) {
        uint32_t day;
        if (competition_round_matchday(g->source,r,&day) != 0) {
            calendar_destroy(c);return -EINVAL;
        }
        c->matchdays[r]=day;
        for (size_t f=0;f<g->fixtures_per_round;++f) {
            fixture_t m=base[(r%g->period)*g->fixtures_per_round+f];
            size_t low=m.home < m.away ? m.home : m.away;
            size_t high=m.home < m.away ? m.away : m.home;
            uint8_t low_at_home=g->orientation[(low*g->n+high)*g->orientation_stride+r/g->period];
            c->fixtures[r*g->fixtures_per_round+f]=
                low_at_home ? (fixture_t){(team_index_t)low,(team_index_t)high}
                            : (fixture_t){(team_index_t)high,(team_index_t)low};
        }
    }
    *out=c;return 0;
}

int calendar_generator_next(calendar_generator_t *g, calendar_t **out,
                            calendar_diagnostics_t *diagnostics) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (g == NULL) return -EINVAL;
    calendar_diagnostics_t stats={0};
    size_t count=g->base_rounds*g->fixtures_per_round;
    const fixture_t *start = g->options.sampling_mode == CALENDAR_SAMPLING_CHAIN
                             ? g->chain_base : g->initial_base;
    fixture_t *base=copy_buffer(start,count,sizeof(*base));
    if (base == NULL) return -ENOMEM;
    mtwister_t rng=g->rng; /* state is failure-atomic */
    int error=0;
    if (g->options.method == CALENDAR_METHOD_ROUND_SHUFFLE) {
        size_t passes = 1U;
        if (g->options.sampling_mode == CALENDAR_SAMPLING_CHAIN) {
            size_t extra = g->generated ? g->options.chain_thinning_moves
                                        : g->options.chain_warmup_moves;
            if (extra == SIZE_MAX) { free(base);return -EOVERFLOW; }
            passes += extra;
        }
        for (size_t pass = 0; pass < passes; ++pass)
            calendar_shuffle_base(g,&rng,base);
    } else {
        size_t moves=g->options.moves_per_sample;
        if (g->options.sampling_mode == CALENDAR_SAMPLING_CHAIN) {
            size_t extra = g->generated ? g->options.chain_thinning_moves
                                        : g->options.chain_warmup_moves;
            if (moves > SIZE_MAX-extra) { free(base);return -EOVERFLOW; }
            moves+=extra;
        }
        error=calendar_recombine_base(g,&rng,base,moves,&stats);
    }
    if (error == 0) {
        for (size_t i=0;i<g->base_rounds;++i) {
            if (!same_matching(base+i*g->fixtures_per_round,
                               g->initial_base+i*g->fixtures_per_round,
                               g->fixtures_per_round)) {
                stats.changed_base_rounds++;
                /* Each base position appears either floor(R/P) or ceil(R/P) times. */
                stats.changed_season_rounds += g->rounds/g->period +
                    (i < (g->rounds%g->period) ? 1U : 0U);
            }
        }
        calendar_t *c=NULL;
        error=materialize(g,base,&c);
        if (error == 0) {
            error=calendar_validate(g->source,c);
            if (error == 0) {
                if (g->options.sampling_mode == CALENDAR_SAMPLING_CHAIN)
                    memcpy(g->chain_base,base,count*sizeof(*base));
                g->rng=rng;g->generated=true;
                *out=c;
                if (diagnostics != NULL) *diagnostics=stats;
            } else calendar_destroy(c);
        }
    }
    free(base);
    return error;
}
