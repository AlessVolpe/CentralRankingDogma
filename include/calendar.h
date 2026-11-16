/*
 * LeaderboardSystem calendar permutation API (milestone 3.0.12).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_CALENDAR_H
#define LEADERBOARD_CALENDAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "competition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct calendar calendar_t;
typedef struct calendar_generator calendar_generator_t;

typedef enum calendar_method {
    CALENDAR_METHOD_ROUND_SHUFFLE = 0,
    CALENDAR_METHOD_RECOMBINE = 1
} calendar_method_t;

typedef enum calendar_sampling_mode {
    CALENDAR_SAMPLING_INDEPENDENT = 0,
    CALENDAR_SAMPLING_CHAIN = 1
} calendar_sampling_mode_t;

typedef struct calendar_options {
    calendar_method_t method;
    calendar_sampling_mode_t sampling_mode;
    uint32_t seed;
    size_t moves_per_sample; /* RECOMBINE attempts; SHUFFLE does one pass */
    bool enable_three_round;
    size_t three_round_attempts; /* randomized search restarts per 3-round move */
    size_t three_round_node_limit; /* max candidate placements per restart */
    size_t chain_warmup_moves; /* extra passes/moves before first CHAIN sample */
    size_t chain_thinning_moves; /* extra passes/moves before later CHAIN samples */
} calendar_options_t;

typedef struct calendar_diagnostics {
    uint64_t two_round_attempts;
    uint64_t two_round_partial_moves;
    uint64_t two_round_whole_swaps;
    uint64_t three_round_attempts;
    uint64_t three_round_moves;
    uint64_t three_round_search_exhaustions;
    uint64_t three_round_nodes;
    size_t changed_base_rounds; /* unordered matchings vs original */
    size_t changed_season_rounds;
} calendar_diagnostics_t;

/* Defaults: recombination, independent mode, explicit seed 5489, 32 moves,
 * bounded 3-round diversification. Supplying explicit options is preferred
 * for reproducible experiments. */
calendar_options_t calendar_options_default(void);

/* Borrows competition until calendar_generator_destroy; the original must
 * already respect the strict N-1-round opponent cooldown / periodicity.
 * -ENOTSUP: valid 3.0.11 snapshot incompatible with that stronger contract.
 * -EINVAL: invalid input/options; -EOVERFLOW: size bounds; -ENOMEM: failure.
 * On ANY error *out=NULL. Pending scores are permitted. */
int calendar_generator_create(const competition_t *competition,
                              const calendar_options_t *options,
                              calendar_generator_t **out);
void calendar_generator_destroy(calendar_generator_t *generator);

/* Each successful call returns an independently owned, immutable calendar.
 * RNG and chain state only advance on success; *out=NULL on failure.
 * A valid result may equal a previously emitted calendar, including original.
 * Diagnostics summarize this generation call only. */
int calendar_generator_next(calendar_generator_t *generator,
                            calendar_t **out,
                            calendar_diagnostics_t *diagnostics);
void calendar_destroy(calendar_t *calendar);
size_t calendar_round_count(const calendar_t *calendar);
size_t calendar_team_count(const calendar_t *calendar);
int calendar_round_matchday(const calendar_t *calendar,
                            size_t round_index, uint32_t *out_matchday);
int calendar_fixture_at(const calendar_t *calendar, size_t round_index,
                        size_t fixture_index, fixture_t *out_fixture);

/* Check the complete schedule independently from the generator: dimensions,
 * team participation, matchday mapping, directed pair counts and cooldown.
 * Returns 0 for valid, -EINVAL for invalid, -ENOMEM/-EOVERFLOW on resource
 * failure. Candidate is interpreted under original's team-index mapping. */
int calendar_validate(const competition_t *original, const calendar_t *candidate);

/* Stable FNV-1a fingerprint of canonical per-round unordered pairings;
 * fixture order and home/away orientation are ignored. This is for sampling
 * diagnostics, NOT collision-proof deduplication. */
uint64_t calendar_fingerprint(const calendar_t *calendar);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_CALENDAR_H */
