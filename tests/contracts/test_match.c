/*
 * LeaderboardSystem milestone 3.1.0 verification.
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Deterministic score and modifier boundaries (milestone 3.1.0). */
#include "match.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do { if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;} } while(0)

int main(void) {
    match_rules_t r;
    CHECK(match_rules_default(&r) == 0);
    CHECK(r.goal_thresholds_tenths[0] == 660U);
    CHECK(r.goal_thresholds_tenths[7] == 940U);
    CHECK(r.post_eighth_step_tenths == 40U);
    CHECK(!r.limit_victory_enabled && !r.limit_draw_enabled);
    match_result_t x;
    for (uint32_t i = 0; i < 8U; ++i) {
        uint32_t t = 660U + i * 40U;
        CHECK(match_resolve_scores(&r,t-1U,0U,&x) == 0 && x.home_goals == i);
        CHECK(match_resolve_scores(&r,t,0U,&x) == 0 && x.home_goals == i+1U);
        CHECK(match_resolve_scores(&r,t+1U,0U,&x) == 0 && x.home_goals == i+1U);
    }
    CHECK(match_resolve_scores(&r,979U,0U,&x) == 0 && x.home_goals == 8U);
    CHECK(match_resolve_scores(&r,980U,0U,&x) == 0 && x.home_goals == 9U);
    CHECK(match_resolve_scores(&r,1020U,0U,&x) == 0 && x.home_goals == 10U);
    CHECK(match_resolve_scores(&r,1060U,0U,&x) == 0 && x.home_goals == 11U);
    CHECK(match_resolve_scores(&r,UINT32_MAX,0U,&x) == 0 && x.home_goals > 11U);
    CHECK(match_rules_uniform(60U,30U,&r) == 0);
    CHECK(r.goal_thresholds_tenths[7] == 1080U);
    CHECK(match_resolve_scores(&r,1109U,0U,&x) == 0 && x.home_goals == 8U);
    CHECK(match_resolve_scores(&r,1110U,0U,&x) == 0 && x.home_goals == 9U);
    CHECK(match_rules_default(&r) == 0);
    r.goal_thresholds_tenths[1] = 685U;
    r.goal_thresholds_tenths[2] = 700U;
    CHECK(match_rules_validate(&r) == 0);
    CHECK(match_resolve_scores(&r,685U,0U,&x) == 0 && x.home_goals == 2U);
    r.goal_thresholds_tenths[2] = 685U;
    CHECK(match_rules_validate(&r) == -EINVAL);
    CHECK(match_rules_default(&r) == 0);
    CHECK(match_rules_default(NULL) == -EINVAL);
    CHECK(match_rules_uniform(0U,40U,&r) == -EINVAL);
    CHECK(match_rules_uniform(UINT32_MAX,40U,&r) == -EINVAL);
    CHECK(match_rules_uniform(40U,0U,&r) == -EINVAL);
    CHECK(match_rules_validate(NULL) == -EINVAL);
    CHECK(match_resolve_scores(NULL,660U,660U,&x) == -EINVAL);
    CHECK(match_resolve_scores(&r,660U,660U,NULL) == -EINVAL);
    CHECK(match_resolve_scores(&r,0U,0U,&x) == 0 && x.home_goals == 0U && x.away_goals == 0U);

    r.limit_victory_enabled = true;
    r.limit_victory_max_gap_tenths = 20U;
    CHECK(match_resolve_scores(&r,700U,680U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 1U &&
          x.adjustment == MATCH_ADJUST_LIMIT_VICTORY);
    CHECK(match_resolve_scores(&r,680U,700U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 1U);
    CHECK(match_resolve_scores(&r,700U,679U,&x) == 0 &&
          x.home_goals == 2U && x.away_goals == 1U);
    CHECK(match_resolve_scores(&r,660U,650U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 0U);
    r.limit_victory_include_subthreshold = true;
    CHECK(match_resolve_scores(&r,660U,650U,&x) == 0 &&
          x.home_goals == 0U && x.away_goals == 0U);
    r.limit_victory_enabled = false;
    r.limit_draw_enabled = true;
    r.limit_draw_min_gap_tenths = 25U;
    CHECK(match_resolve_scores(&r,699U,674U,&x) == 0 &&
          x.home_goals == 2U && x.away_goals == 1U &&
          x.adjustment == MATCH_ADJUST_LIMIT_DRAW);
    CHECK(match_resolve_scores(&r,674U,699U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 2U);
    CHECK(match_resolve_scores(&r,699U,675U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 1U);
    CHECK(match_resolve_scores(&r,650U,620U,&x) == 0 &&
          x.home_goals == 0U && x.away_goals == 0U);
    r.limit_draw_include_subthreshold = true;
    CHECK(match_resolve_scores(&r,650U,620U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 0U);
    CHECK(match_resolve_scores(&r,670U,670U,&x) == 0 &&
          x.home_goals == 1U && x.away_goals == 1U);
    /* The pure function does not write outputs when given invalid rules. */
    match_result_t before = x;
    r.goal_thresholds_tenths[0] = 659U;
    CHECK(match_resolve_scores(&r,700U,650U,&x) == -EINVAL);
    CHECK(memcmp(&before,&x,sizeof(x)) == 0);
    return EXIT_SUCCESS;
}
