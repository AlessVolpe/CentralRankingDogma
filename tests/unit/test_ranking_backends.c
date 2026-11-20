/* Exact heapsort/AVL season and Monte Carlo differential verification. */
#include "season_resolution.h"
#include "monte_carlo.h"
#include "calendar_test_fixture.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}}while(0)
static int compare_seasons(const season_resolution_t *a,const season_resolution_t *b) {
    const size_t n=season_resolution_team_count(a);
    CHECK(n==season_resolution_team_count(b));
    CHECK(season_resolution_round_count(a)==season_resolution_round_count(b));
    CHECK(season_resolution_fixture_count(a)==season_resolution_fixture_count(b));
    for(size_t pos=0U;pos<n;++pos) {
        season_rank_entry_t x,y;
        CHECK(season_resolution_rank_at(a,pos,&x)==0);
        CHECK(season_resolution_rank_at(b,pos,&y)==0);
        CHECK(x.team_index==y.team_index && x.sporting_rank==y.sporting_rank);
        CHECK(x.fantasy_total_tenths==y.fantasy_total_tenths && x.goal_difference==y.goal_difference);
        CHECK(x.standings.championship_points==y.standings.championship_points);
        CHECK(x.standings.wins==y.standings.wins && x.standings.draws==y.standings.draws);
        CHECK(x.standings.losses==y.standings.losses);
        CHECK(x.goal_stats.goals_for==y.goal_stats.goals_for);
        CHECK(x.goal_stats.goals_against==y.goal_stats.goals_against);
        CHECK(x.goal_stats.scored_matches==y.goal_stats.scored_matches);
        CHECK(x.goal_stats.complete==y.goal_stats.complete);
        team_uuid_t u,v;
        CHECK(season_resolution_team_uuid_at(a,(team_index_t)pos,&u)==0);
        CHECK(season_resolution_team_uuid_at(b,(team_index_t)pos,&v)==0);
        CHECK(team_uuid_compare(&u,&v)==0);
    }
    for(size_t fi=0U;fi<season_resolution_fixture_count(a);++fi) {
        season_fixture_result_t x,y;
        CHECK(season_resolution_fixture_at(a,fi,&x)==0);
        CHECK(season_resolution_fixture_at(b,fi,&y)==0);
        CHECK(x.round_index==y.round_index && x.fixture_index==y.fixture_index);
        CHECK(x.serie_a_matchday==y.serie_a_matchday);
        CHECK(x.fixture.home==y.fixture.home && x.fixture.away==y.fixture.away);
        CHECK(x.result.home_goals==y.result.home_goals);
        CHECK(x.result.away_goals==y.result.away_goals);
    }
    return EXIT_SUCCESS;
}
static int compare_calendar(const competition_t *c,const calendar_t *calendar,
                            const match_rules_t *rules,season_resolution_scope_t scope) {
    season_resolution_options_t hs={scope,SEASON_RANKING_HEAPSORT};
    season_resolution_options_t avl={scope,SEASON_RANKING_AVL};
    season_resolution_t *a=NULL,*b=NULL,*legacy=NULL;
    CHECK(season_resolve_with_options(c,calendar,rules,&hs,&a)==0);
    CHECK(season_resolve_with_options(c,calendar,rules,&avl,&b)==0);
    CHECK(season_resolve(c,calendar,rules,scope,&legacy)==0);
    CHECK(compare_seasons(a,b)==EXIT_SUCCESS);
    CHECK(compare_seasons(a,legacy)==EXIT_SUCCESS);
    season_resolution_destroy(a);season_resolution_destroy(b);season_resolution_destroy(legacy);
    return EXIT_SUCCESS;
}
static int compare_streams(FILE *a,FILE *b) {
    CHECK(fseek(a,0L,SEEK_SET)==0 && fseek(b,0L,SEEK_SET)==0);
    int x,y;do {x=fgetc(a);y=fgetc(b);CHECK(x==y);} while(x!=EOF);
    return EXIT_SUCCESS;
}
static int test_group(size_t teams,size_t rounds,int tied) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,teams,rounds)==0);
    if(tied) for(size_t i=0U;i<teams*rounds;++i)
        f.scores[i].score=(fantasy_score_t){660U,FANTASY_SCORE_FINAL};
    competition_t *c=NULL;
    CHECK(competition_create(&f.input,&c)==0);
    match_rules_t rules;CHECK(match_rules_default(&rules)==0);
    season_resolution_scope_t scope=tied ? SEASON_RESOLVE_FULL_SEASON : SEASON_RESOLVE_FINALIZED_PREFIX;
    CHECK(compare_calendar(c,NULL,&rules,scope)==EXIT_SUCCESS);
    calendar_options_t opts=calendar_options_default();
    opts.seed=592U;opts.method=teams<=4U ? CALENDAR_METHOD_ROUND_SHUFFLE : CALENDAR_METHOD_RECOMBINE;
    calendar_generator_t *gen=NULL;
    CHECK(calendar_generator_create(c,&opts,&gen)==0);
    for(size_t i=0U;i<12U;++i) {
        calendar_t *candidate=NULL;
        CHECK(calendar_generator_next(gen,&candidate,NULL)==0);
        CHECK(compare_calendar(c,candidate,&rules,scope)==EXIT_SUCCESS);
        calendar_destroy(candidate);
    }
    calendar_generator_destroy(gen);
    if(tied) {
        season_resolution_options_t avl={scope,SEASON_RANKING_AVL};
        season_resolution_t *all=NULL;
        CHECK(season_resolve_with_options(c,NULL,&rules,&avl,&all)==0);
        for(size_t i=0U;i<teams;++i) {
            season_rank_entry_t item;
            CHECK(season_resolution_rank_at(all,i,&item)==0);
            CHECK(item.team_index==i && item.sporting_rank==1U);
        }
        season_resolution_destroy(all);
    }
    if(!tied) {
        monte_carlo_options_t config=monte_carlo_options_default();
        config.calendar.seed=1441U;
        config.calendar.method=CALENDAR_METHOD_RECOMBINE;
        config.requested_samples=config.max_generation_attempts=8U;
        FILE *raw_a=tmpfile(),*raw_b=tmpfile(),*sum_a=tmpfile(),*sum_b=tmpfile();
        CHECK(raw_a!=NULL && raw_b!=NULL && sum_a!=NULL && sum_b!=NULL);
        monte_carlo_result_t *a=NULL,*b=NULL;
        CHECK(monte_carlo_run(c,&rules,&config,raw_a,NULL,NULL,&a)==0);
        config.ranking_backend=SEASON_RANKING_AVL;
        CHECK(monte_carlo_run(c,&rules,&config,raw_b,NULL,NULL,&b)==0);
        CHECK(monte_carlo_result_attempts(a)==monte_carlo_result_attempts(b));
        CHECK(monte_carlo_result_accepted(a)==monte_carlo_result_accepted(b));
        CHECK(compare_seasons(monte_carlo_result_baseline(a),
                              monte_carlo_result_baseline(b))==EXIT_SUCCESS);
        CHECK(compare_streams(raw_a,raw_b)==EXIT_SUCCESS);
        CHECK(monte_carlo_write_summary_csv(a,sum_a)==0);
        CHECK(monte_carlo_write_summary_csv(b,sum_b)==0);
        CHECK(compare_streams(sum_a,sum_b)==EXIT_SUCCESS);
        monte_carlo_result_destroy(a);monte_carlo_result_destroy(b);
        fclose(raw_a);fclose(raw_b);fclose(sum_a);fclose(sum_b);
    }
    season_resolution_options_t invalid={scope,(season_ranking_backend_t)77};
    season_resolution_t *bad=(season_resolution_t *)(uintptr_t)1U;
    CHECK(season_resolve_with_options(c,NULL,&rules,&invalid,&bad)==-EINVAL && bad==NULL);
    bad=(season_resolution_t *)(uintptr_t)1U;
    CHECK(season_resolve_with_options(c,NULL,&rules,NULL,&bad)==-EINVAL && bad==NULL);
    monte_carlo_options_t mc=monte_carlo_options_default();
    mc.ranking_backend=(season_ranking_backend_t)77;
    monte_carlo_result_t *not_created=(monte_carlo_result_t *)(uintptr_t)1U;
    CHECK(monte_carlo_run(c,&rules,&mc,NULL,NULL,NULL,&not_created)==-EINVAL && not_created==NULL);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
int main(void) {
    CHECK(test_group(4U,3U,1)==EXIT_SUCCESS);
    CHECK(test_group(4U,9U,0)==EXIT_SUCCESS);
    CHECK(test_group(8U,14U,0)==EXIT_SUCCESS);
    CHECK(test_group(10U,35U,0)==EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
