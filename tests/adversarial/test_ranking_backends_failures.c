/* AVL season resolver and Monte Carlo fault-injection transaction checks. */
#include "season_resolution.h"
#include "monte_carlo.h"
#include "calendar_test_fixture.h"
#include "alloc_internal.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}}while(0)
int main(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,3U)==0);
    competition_t *c=NULL;CHECK(competition_create(&f.input,&c)==0);
    match_rules_t rules;CHECK(match_rules_default(&rules)==0);
    season_resolution_options_t opts={SEASON_RESOLVE_FULL_SEASON,SEASON_RANKING_AVL};
    size_t failures=0U;int completed=0;
    for(size_t i=0U;i<128U;++i) {
        season_resolution_t *r=(season_resolution_t *)(uintptr_t)1U;
        leaderboard_alloc_fail_after(i);
        const int rc=season_resolve_with_options(c,NULL,&rules,&opts,&r);
        leaderboard_alloc_reset();
        if(rc==-ENOMEM) {CHECK(r==NULL);++failures;}
        else {CHECK(rc==0 && r!=NULL);season_resolution_destroy(r);completed=1;break;}
    }
    CHECK(completed && failures>=15U);
    monte_carlo_options_t mc=monte_carlo_options_default();
    mc.scope=SEASON_RESOLVE_FULL_SEASON;
    mc.ranking_backend=SEASON_RANKING_AVL;
    mc.calendar.method=CALENDAR_METHOD_ROUND_SHUFFLE;
    mc.requested_samples=mc.max_generation_attempts=1U;
    completed=0;failures=0U;
    for(size_t i=0U;i<256U;++i) {
        monte_carlo_result_t *r=(monte_carlo_result_t *)(uintptr_t)1U;
        leaderboard_alloc_fail_after(i);
        const int rc=monte_carlo_run(c,&rules,&mc,NULL,NULL,NULL,&r);
        leaderboard_alloc_reset();
        if(rc==-ENOMEM) {CHECK(r==NULL);++failures;}
        else {CHECK(rc==0 && r!=NULL);CHECK(monte_carlo_result_accepted(r)==1U);
              monte_carlo_result_destroy(r);completed=1;break;}
    }
    CHECK(completed && failures>=20U);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
