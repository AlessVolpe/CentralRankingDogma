/*
 * LeaderboardSystem deterministic season evaluation (milestone 3.1.0).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_SEASON_RESOLUTION_H
#define LEADERBOARD_SEASON_RESOLUTION_H

#include <stddef.h>
#include <stdint.h>
#include "calendar.h"
#include "match.h"
#include "standings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum season_resolution_scope {
    SEASON_RESOLVE_FINALIZED_PREFIX = 0,
    SEASON_RESOLVE_FULL_SEASON = 1
} season_resolution_scope_t;

/* Ranking backend is an algorithm selection, not a sporting rule change.
 * HEAPSORT is the existing 3.1.0 reference and remains the default. */
typedef enum season_ranking_backend {
    SEASON_RANKING_HEAPSORT = 0,
    SEASON_RANKING_AVL = 1
} season_ranking_backend_t;

typedef struct season_resolution_options {
    season_resolution_scope_t scope;
    season_ranking_backend_t ranking_backend;
} season_resolution_options_t;

typedef struct season_resolution season_resolution_t;

typedef struct season_fixture_result {
    size_t round_index;
    size_t fixture_index;
    uint32_t serie_a_matchday;
    fixture_t fixture;
    match_result_t result;
} season_fixture_result_t;

typedef struct season_rank_entry {
    team_index_t team_index;
    uint32_t sporting_rank; /* 1,1,3; UUID order for display only */
    uint32_t fantasy_total_tenths; /* sum over resolved rounds only */
    int64_t goal_difference;
    standing_entry_t standings;
    standings_goal_stats_t goal_stats;
} season_rank_entry_t;

/* NULL selected_calendar means the original competition fixtures.
 * FULL_SEASON requires every score FINAL (-EAGAIN otherwise).
 * FINALIZED_PREFIX stops at the first round with any PENDING score.
 * A successfully returned result owns all data and survives destruction
 * of competition, selected_calendar, and rules.
 * All failure paths leave *out NULL; no partial result escapes. */
int season_resolve(const competition_t *competition,
                   const calendar_t *selected_calendar,
                   const match_rules_t *rules,
                   season_resolution_scope_t scope,
                   season_resolution_t **out);
/* Options are borrowed during the call. The selected backend must produce
 * the same owned result, sporting ties and error-atomic semantics as the
 * legacy heapsort. NULL or unrecognized options return -EINVAL with *out=NULL. */
int season_resolve_with_options(const competition_t *competition,
                                const calendar_t *selected_calendar,
                                const match_rules_t *rules,
                                const season_resolution_options_t *options,
                                season_resolution_t **out);
void season_resolution_destroy(season_resolution_t *resolution);
size_t season_resolution_round_count(const season_resolution_t *resolution);
size_t season_resolution_fixture_count(const season_resolution_t *resolution);
size_t season_resolution_team_count(const season_resolution_t *resolution);
int season_resolution_fixture_at(const season_resolution_t *resolution,
                                 size_t index, season_fixture_result_t *out);
int season_resolution_rank_at(const season_resolution_t *resolution,
                              size_t position, season_rank_entry_t *out);
int season_resolution_team_uuid_at(const season_resolution_t *resolution,
                                   team_index_t index, team_uuid_t *out);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_SEASON_RESOLUTION_H */
