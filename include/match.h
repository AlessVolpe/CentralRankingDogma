/*
 * LeaderboardSystem deterministic fantasy match scoring (milestone 3.1.0).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_MATCH_H
#define LEADERBOARD_MATCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct match_rules {
    uint32_t goal_thresholds_tenths[8]; /* first exactly 660; strictly rising */
    uint32_t post_eighth_step_tenths; /* strictly positive */
    bool limit_victory_enabled;
    uint32_t limit_victory_max_gap_tenths;
    bool limit_victory_include_subthreshold;
    bool limit_draw_enabled;
    uint32_t limit_draw_min_gap_tenths;
    bool limit_draw_include_subthreshold;
} match_rules_t;

typedef enum match_adjustment {
    MATCH_ADJUST_NONE = 0,
    MATCH_ADJUST_LIMIT_VICTORY = 1,
    MATCH_ADJUST_LIMIT_DRAW = 2
} match_adjustment_t;

typedef struct match_result {
    uint32_t home_fantasy_tenths;
    uint32_t away_fantasy_tenths;
    uint32_t home_goals;
    uint32_t away_goals;
    match_adjustment_t adjustment;
} match_result_t;

/* Exact default: 66,70,...,94, then +4 for every further goal.
 * Both optional modifiers are disabled; no home advantage. */
int match_rules_default(match_rules_t *out);
/* Uniform steps from goal 2 through 8, and an independent post-8 step.
 * Reject overflow rather than wrapping a threshold. */
int match_rules_uniform(uint32_t step_through_eighth_tenths,
                        uint32_t post_eighth_step_tenths,
                        match_rules_t *out);
/* Enforces valid thresholds and enabled modifier options. */
int match_rules_validate(const match_rules_t *rules);
/* Allocation-free, non-mutating, error-atomic. Scores must be FINAL values;
 * no score status can be represented by this low-level API. */
int match_resolve_scores(const match_rules_t *rules,
                         uint32_t home_score_tenths,
                         uint32_t away_score_tenths,
                         match_result_t *out);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_MATCH_H */
