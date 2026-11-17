/*
 * LeaderboardSystem milestone 3.1.0 verification.
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Contract and official-workbook regression for milestone 3.1.0. */
#include "season_resolution.h"
#include "calendar_test_fixture.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do { if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;} } while(0)

static int simple(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,9U) == 0);
    competition_t *c = NULL;
    CHECK(competition_create(&f.input,&c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    season_resolution_t *r = (season_resolution_t *)(uintptr_t)1U;
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FULL_SEASON,&r) == -EAGAIN && r == NULL);
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FINALIZED_PREFIX,&r) == 0);
    CHECK(season_resolution_round_count(r) == 8U);
    CHECK(season_resolution_fixture_count(r) == 16U);
    CHECK(season_resolution_team_count(r) == 4U);
    for (size_t i=0U;i<16U;++i) {
        season_fixture_result_t rec;
        CHECK(season_resolution_fixture_at(r,i,&rec) == 0);
        CHECK(rec.round_index == i/2U);
        CHECK(rec.serie_a_matchday == 4U + i/2U);
        CHECK(rec.result.home_fantasy_tenths >= 600U);
    }
    CHECK(season_resolution_fixture_at(r,16U,&(season_fixture_result_t){0}) == -ERANGE);
    CHECK(season_resolution_rank_at(r,4U,&(season_rank_entry_t){0}) == -ERANGE);
    for (size_t i=0U;i<4U;++i) {
        season_rank_entry_t e;
        CHECK(season_resolution_rank_at(r,i,&e) == 0);
        CHECK(e.standings.wins + e.standings.draws + e.standings.losses == 8U);
        CHECK(e.goal_stats.complete && e.goal_stats.scored_matches == 8U);
        CHECK(e.fantasy_total_tenths > 0U);
    }
    /* Independent owned output survives the destruction of its input. */
    competition_destroy(c);
    season_rank_entry_t first;
    CHECK(season_resolution_rank_at(r,0U,&first) == 0);
    season_resolution_destroy(r);
    return EXIT_SUCCESS;
}

static int ties_and_gaps(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,4U) == 0);
    for (size_t i=0U;i<16U;++i) {
        f.scores[i].score = (fantasy_score_t){660U,FANTASY_SCORE_FINAL};
    }
    /* A finalized score after a gap must not contaminate prefix tie-breakers. */
    f.scores[4U].score = (fantasy_score_t){0U,FANTASY_SCORE_PENDING};
    f.scores[8U].score = (fantasy_score_t){900U,FANTASY_SCORE_FINAL};
    competition_t *c=NULL;
    CHECK(competition_create(&f.input,&c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    season_resolution_t *r=NULL;
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FINALIZED_PREFIX,&r) == 0);
    CHECK(season_resolution_round_count(r) == 1U);
    CHECK(season_resolution_fixture_count(r) == 2U);
    for (size_t i=0U;i<4U;++i) {
        season_rank_entry_t e;
        CHECK(season_resolution_rank_at(r,i,&e) == 0);
        CHECK(e.fantasy_total_tenths == 660U);
        CHECK(e.sporting_rank == 1U);
        CHECK(e.team_index == (team_index_t)i);
    }
    season_resolution_destroy(r);
    competition_destroy(c);
    /* Empty prefixes are valid and have one shared first place. */
    f.scores[0U].score = (fantasy_score_t){0U,FANTASY_SCORE_PENDING};
    CHECK(competition_create(&f.input,&c) == 0);
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FINALIZED_PREFIX,&r) == 0);
    CHECK(season_resolution_fixture_count(r) == 0U);
    CHECK(season_resolution_rank_at(r,0U,&(season_rank_entry_t){0}) == 0);
    season_resolution_destroy(r);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

static int generated_calendar(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,9U)==0);
    competition_t *c=NULL;
    CHECK(competition_create(&f.input,&c)==0);
    calendar_options_t opts=calendar_options_default();
    opts.method=CALENDAR_METHOD_RECOMBINE;
    opts.seed=57U;
    calendar_generator_t *gen=NULL;
    CHECK(calendar_generator_create(c,&opts,&gen)==0);
    calendar_t *candidate=NULL;
    CHECK(calendar_generator_next(gen,&candidate,NULL)==0 && candidate!=NULL);
    CHECK(calendar_validate(c,candidate)==0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules)==0);
    season_resolution_t *base=NULL,*alternative=NULL;
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FINALIZED_PREFIX,&base)==0);
    CHECK(season_resolve(c,candidate,&rules,SEASON_RESOLVE_FINALIZED_PREFIX,&alternative)==0);
    CHECK(season_resolution_fixture_count(base)==season_resolution_fixture_count(alternative));
    for(size_t ti=0U;ti<4U;++ti) {
        uint32_t fantasy_base=UINT32_MAX, fantasy_alt=UINT32_MAX;
        for(size_t pos=0U;pos<4U;++pos) {
            season_rank_entry_t rank;
            CHECK(season_resolution_rank_at(base,pos,&rank)==0);
            if(rank.team_index==ti) fantasy_base=rank.fantasy_total_tenths;
            CHECK(season_resolution_rank_at(alternative,pos,&rank)==0);
            if(rank.team_index==ti) fantasy_alt=rank.fantasy_total_tenths;
        }
        CHECK(fantasy_base==fantasy_alt && fantasy_base!=UINT32_MAX);
    }
    calendar_destroy(candidate);
    calendar_generator_destroy(gen);
    competition_destroy(c);
    season_rank_entry_t top;
    CHECK(season_resolution_rank_at(alternative,0U,&top)==0);
    season_resolution_destroy(base);
    season_resolution_destroy(alternative);
    return EXIT_SUCCESS;
}

static int competition_rank_ties(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,1U)==0);
    const uint32_t points[4]={660U,700U,650U,660U};
    for(size_t ti=0U;ti<4U;++ti)
        f.scores[ti].score=(fantasy_score_t){points[ti],FANTASY_SCORE_FINAL};
    competition_t *c=NULL;
    CHECK(competition_create(&f.input,&c)==0);
    match_rules_t rule;
    CHECK(match_rules_default(&rule)==0);
    season_resolution_t *r=NULL;
    CHECK(season_resolve(c,NULL,&rule,SEASON_RESOLVE_FULL_SEASON,&r)==0);
    const uint32_t ranks[]={1U,2U,2U,4U};
    const team_index_t ordered[]={1U,0U,3U,2U};
    for(size_t i=0U;i<4U;++i) {
        season_rank_entry_t e;
        CHECK(season_resolution_rank_at(r,i,&e)==0);
        CHECK(e.sporting_rank==ranks[i] && e.team_index==ordered[i]);
    }
    season_resolution_destroy(r);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

/* Expected goals are independent recorded vectors from the 66/4 historical
 * workbook baseline (not computed by calling match_resolve_scores). */
static const uint32_t expected[40][2] = {
    {0U,1U}, {0U,1U}, {2U,2U}, {8U,1U}, {3U,3U},
    {0U,0U}, {3U,3U}, {3U,3U}, {3U,2U}, {2U,0U},
    {4U,0U}, {1U,1U}, {1U,1U}, {2U,2U}, {4U,1U},
    {4U,6U}, {0U,0U}, {0U,1U}, {2U,3U}, {0U,0U},
    {3U,0U}, {0U,3U}, {2U,2U}, {4U,5U}, {2U,0U},
    {3U,2U}, {0U,7U}, {4U,0U}, {1U,6U}, {2U,2U},
    {3U,0U}, {3U,3U}, {1U,2U}, {3U,2U}, {5U,3U},
    {2U,0U}, {1U,3U}, {2U,0U}, {5U,1U}, {3U,3U}
};

static int workbook(void) {
#ifndef LEADERBOARD_CALENDAR_TSV
#error "LEADERBOARD_CALENDAR_TSV required"
#endif
    FILE *file = fopen(LEADERBOARD_CALENDAR_TSV,"rb");
    CHECK(file != NULL);
    char ids[10][TEAM_UUID_TEXT_SIZE];
    char names[10][TEAM_NAME_MAX_BYTES+1];
    competition_team_input_t teams[10];
    competition_fixture_input_t matches[35][5];
    competition_round_input_t rounds[35] = {{0}};
    competition_score_input_t scores[350] = {{0}};
    char line[1024];
    size_t team_count=0U, fixture_count=0U;
    while (fgets(line,sizeof(line),file) != NULL) {
        if (line[0] == '#') continue;
        unsigned rd,day,p1,p2;
        char home[256],away[256],stat;
        CHECK(sscanf(line,"%u\t%u\t%255[^\t]\t%255[^\t]\t%u\t%u\t%c",
                     &rd,&day,home,away,&p1,&p2,&stat) == 7);
        CHECK(rd>=1U && rd<=35U);
        const char *parts[2] = {home,away};
        size_t idx[2];
        for (size_t k=0U;k<2U;++k) {
            size_t j=0U;
            while (j<team_count && strcmp(names[j],parts[k]) != 0) ++j;
            if (j==team_count) {
                CHECK(team_count<10U);
                (void)snprintf(names[j],sizeof(names[j]),"%s",parts[k]);
                (void)snprintf(ids[j],sizeof(ids[j]),
                               "00000000-0000-4000-8000-%012u",(unsigned)(j+1U));
                teams[j] = (competition_team_input_t){ids[j],names[j]};
                ++team_count;
            }
            idx[k]=j;
        }
        size_t ri=(size_t)(rd-1U), fi=rounds[ri].fixture_count;
        CHECK(fi<5U);
        matches[ri][fi] = (competition_fixture_input_t){ids[idx[0]],ids[idx[1]]};
        rounds[ri].fixture_count++;
        rounds[ri].serie_a_matchday=day;
        CHECK(stat=='F' || stat=='P');
        scores[ri*10U+idx[0]] = (competition_score_input_t){ids[idx[0]],day,
            {p1,stat=='F'?FANTASY_SCORE_FINAL:FANTASY_SCORE_PENDING}};
        scores[ri*10U+idx[1]] = (competition_score_input_t){ids[idx[1]],day,
            {p2,stat=='F'?FANTASY_SCORE_FINAL:FANTASY_SCORE_PENDING}};
        ++fixture_count;
    }
    CHECK(fclose(file)==0 && fixture_count==175U && team_count==10U);
    for(size_t ri=0U;ri<35U;++ri) {
        CHECK(rounds[ri].fixture_count==5U);
        rounds[ri].fixtures=matches[ri];
    }
    competition_input_t input={teams,10U,rounds,35U,scores,350U};
    competition_t *c=NULL;
    CHECK(competition_create(&input,&c)==0);
    match_rules_t rule;
    CHECK(match_rules_default(&rule)==0);
    season_resolution_t *r=NULL;
    CHECK(season_resolve(c,NULL,&rule,SEASON_RESOLVE_FINALIZED_PREFIX,&r)==0);
    CHECK(season_resolution_round_count(r)==8U);
    CHECK(season_resolution_fixture_count(r)==40U);
    uint64_t goals=0U,points=0U;
    for(size_t i=0U;i<40U;++i) {
        season_fixture_result_t rec;
        CHECK(season_resolution_fixture_at(r,i,&rec)==0);
        CHECK(rec.result.home_goals==expected[i][0]);
        CHECK(rec.result.away_goals==expected[i][1]);
    }
    const char *order[10]={"Goodfellas fc","Schalke 104","CIRCO MASSIMO FC",
        "Fortuna Duxeldorf","fc paros","Boh","Brigate Ebosse",
        "VolevoMalen","Racing de SanDeAndre","Paralympiakos"};
    const uint32_t expected_points[10]={20U,17U,16U,11U,10U,9U,8U,7U,6U,2U};
    const int64_t expected_gd[10]={14,12,8,3,2,-10,-11,-1,-2,-15};
    const uint32_t fantasy[10]={6160U,5725U,6070U,5860U,5790U,5760U,
                                  5510U,5825U,5685U,5160U};
    for(size_t i=0U;i<10U;++i) {
        season_rank_entry_t rank;
        CHECK(season_resolution_rank_at(r,i,&rank)==0);
        char actual[TEAM_NAME_MAX_BYTES+1];
        (void)snprintf(actual,sizeof(actual),"%s",names[rank.team_index]);
        /* Indices are UUID-sorted and local fixture UUIDs were assigned
         * in insertion order by the workbook loader. */
        CHECK(strcmp(actual,order[i])==0);
        CHECK(rank.sporting_rank==(uint32_t)(i+1U));
        CHECK(rank.standings.championship_points==expected_points[i]);
        CHECK(rank.goal_difference==expected_gd[i]);
        CHECK(rank.fantasy_total_tenths==fantasy[i]);
        CHECK(rank.goal_stats.complete && rank.goal_stats.scored_matches==8U);
        goals+=rank.goal_stats.goals_for;
        points+=rank.standings.championship_points;
    }
    CHECK(goals==166U && points==106U);
    season_resolution_destroy(r);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

int main(void) {
    CHECK(simple()==0);
    CHECK(ties_and_gaps()==0);
    CHECK(generated_calendar()==0);
    CHECK(competition_rank_ties()==0);
    CHECK(workbook()==0);
    return EXIT_SUCCESS;
}
