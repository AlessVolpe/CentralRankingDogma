/*
 * LeaderboardSystem deterministic fantasy match scoring (milestone 3.1.0).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "match.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>

int match_rules_default(match_rules_t *out) {
    return match_rules_uniform(40U, 40U, out);
}

int match_rules_uniform(uint32_t step, uint32_t after_eight, match_rules_t *out) {
    if (out == NULL || step == 0U || after_eight == 0U ||
        step > (UINT32_MAX - 660U) / 7U) return -EINVAL;
    match_rules_t rules = {0};
    for (size_t i = 0U; i < 8U; ++i)
        rules.goal_thresholds_tenths[i] = 660U + (uint32_t)i * step;
    rules.post_eighth_step_tenths = after_eight;
    *out = rules;
    return 0;
}

int match_rules_validate(const match_rules_t *r) {
    if (r == NULL || r->goal_thresholds_tenths[0] != 660U ||
        r->post_eighth_step_tenths == 0U) return -EINVAL;
    for (size_t i = 1U; i < 8U; ++i)
        if (r->goal_thresholds_tenths[i] <= r->goal_thresholds_tenths[i-1U])
            return -EINVAL;
    /* An enabled modifier with a zero difference threshold is meaningful:
     * <=0 for victory and >=0 for draw, when goal bands qualify. */
    return 0;
}

static uint32_t basic_goals(const match_rules_t *r, uint32_t score) {
    uint32_t goals = 0U;
    for (size_t i = 0U; i < 8U; ++i)
        if (score >= r->goal_thresholds_tenths[i]) ++goals;
    if (score >= r->goal_thresholds_tenths[7])
        goals += (score - r->goal_thresholds_tenths[7]) /
                 r->post_eighth_step_tenths;
    return goals;
}

int match_resolve_scores(const match_rules_t *rules, uint32_t hs,
                         uint32_t as, match_result_t *out) {
    if (out == NULL || match_rules_validate(rules) != 0) return -EINVAL;
    const uint32_t home = basic_goals(rules, hs);
    const uint32_t away = basic_goals(rules, as);
    match_result_t next = {hs, as, home, away, MATCH_ADJUST_NONE};
    if (hs != as) {
        const bool higher_home = hs > as;
        const uint32_t gap = higher_home ? hs - as : as - hs;
        const uint32_t higher_goals = higher_home ? home : away;
        const uint32_t lower_goals = higher_home ? away : home;
        if (rules->limit_victory_enabled &&
            higher_goals == lower_goals + 1U &&
            gap <= rules->limit_victory_max_gap_tenths &&
            (lower_goals != 0U || rules->limit_victory_include_subthreshold)) {
            if (higher_home) --next.home_goals;
            else --next.away_goals;
            next.adjustment = MATCH_ADJUST_LIMIT_VICTORY;
        } else if (rules->limit_draw_enabled && higher_goals == lower_goals &&
                   gap >= rules->limit_draw_min_gap_tenths &&
                   (higher_goals != 0U || rules->limit_draw_include_subthreshold)) {
            /* basic_goals() is bounded for a uint32_t score and positive
             * threshold spacing, leaving room for a modifier increment. */
            if (higher_home) ++next.home_goals;
            else ++next.away_goals;
            next.adjustment = MATCH_ADJUST_LIMIT_DRAW;
        }
    }
    *out = next;
    return 0;
}
