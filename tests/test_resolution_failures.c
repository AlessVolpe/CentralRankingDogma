/*
 * LeaderboardSystem milestone 3.1.0 verification.
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Rollback, goal-state compatibility and injected allocation failures. */
#include "season_resolution.h"
#include "calendar_test_fixture.h"
#include "standings_internal.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}} while(0)

int main(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,3U)==0);
    /* Make every input FINAL for the full-season and failure tests. */
    competition_t *c=NULL;
    CHECK(competition_create(&f.input,&c)==0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules)==0);

    /* Check every allocation site in the new resolver and standings. */
    size_t failures=0U;
    for(size_t at=0U;at<25U;++at) {
        season_resolution_t *r=(season_resolution_t *)(uintptr_t)1U;
        leaderboard_alloc_fail_after(at);
        int status=season_resolve(c,NULL,&rules,SEASON_RESOLVE_FULL_SEASON,&r);
        leaderboard_alloc_reset();
        if(status==-ENOMEM) {CHECK(r==NULL);++failures;}
        else {CHECK(status==0 && r!=NULL);season_resolution_destroy(r);break;}
    }
    CHECK(failures>=6U);
    standings_t *s=NULL;
    CHECK(standings_create(c,&s)==0);
    standings_goal_stats_t gs;
    CHECK(standings_goal_stats_at(s,0U,&gs)==0 && gs.complete && gs.scored_matches==0U);
    CHECK(standings_record_scored_match(s,0U,1U,2U,1U)==0);
    CHECK(standings_goal_stats_at(s,0U,&gs)==0 && gs.complete &&
          gs.scored_matches==1U && gs.goals_for==2U && gs.goals_against==1U);
    CHECK(standings_record_match(s,0U,2U,STANDINGS_DRAW)==0);
    CHECK(standings_goal_stats_at(s,0U,&gs)==0 && !gs.complete && gs.scored_matches==1U);
    CHECK(standings_goal_stats_at(s,2U,&gs)==0 && !gs.complete && gs.scored_matches==0U);
    CHECK(standings_record_result(s,3U,STANDINGS_WIN)==0);
    CHECK(standings_goal_stats_at(s,3U,&gs)==0 && !gs.complete);
    standings_reset(s);
    CHECK(standings_goal_stats_at(s,0U,&gs)==0 && gs.complete && gs.goals_for==0U);
    CHECK(standings_record_scored_match(s,0U,0U,1U,2U)==-EINVAL);
    CHECK(standings_record_scored_match(s,0U,4U,1U,2U)==-ERANGE);
    CHECK(standings_goal_stats_at(s,4U,&gs)==-ERANGE);
    CHECK(standings_goal_stats_at(s,0U,NULL)==-EINVAL);
    CHECK(standings_record_scored_match(NULL,0U,1U,1U,2U)==-EINVAL);
    /* Failure on the away goals/counter must not commit the home update. */
    s->goals[1].goals_for=UINT32_MAX;
    standing_entry_t h0=s->entries[0],h1=s->entries[1];
    standings_goal_stats_t g0=s->goals[0],g1=s->goals[1];
    CHECK(standings_record_scored_match(s,0U,1U,0U,1U)==-EOVERFLOW);
    CHECK(memcmp(&h0,&s->entries[0],sizeof(h0))==0);
    CHECK(memcmp(&h1,&s->entries[1],sizeof(h1))==0);
    CHECK(memcmp(&g0,&s->goals[0],sizeof(g0))==0);
    CHECK(memcmp(&g1,&s->goals[1],sizeof(g1))==0);
    /* Ensure a scoring overflow on the home team also aborts both. */
    standings_reset(s);
    s->entries[0].wins=UINT32_MAX;
    CHECK(standings_record_scored_match(s,0U,1U,2U,1U)==-EOVERFLOW);
    CHECK(s->entries[1].wins==0U && s->entries[1].losses==0U);
    standings_destroy(s);

    season_resolution_t *r=(season_resolution_t *)(uintptr_t)1U;
    CHECK(season_resolve(c,NULL,&rules,(season_resolution_scope_t)99,&r)==-EINVAL && r==NULL);
    CHECK(season_resolve(NULL,NULL,&rules,SEASON_RESOLVE_FULL_SEASON,&r)==-EINVAL && r==NULL);
    CHECK(season_resolve(c,NULL,NULL,SEASON_RESOLVE_FULL_SEASON,&r)==-EINVAL && r==NULL);
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FULL_SEASON,NULL)==-EINVAL);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
