/*
 * LeaderboardSystem installed-package consumer (milestone 3.0.7).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ranking.h"
#include "mtwister.h"
#include "team.h"
#include "competition.h"
#include "standings.h"
#include "calendar.h"
#include "match.h"
#include "season_resolution.h"
#include "monte_carlo.h"
#include "hashmap_typed.h"

HASHMAP_TYPED_DEFINE(external_score_map, char, int)

static int external_key_compare(const void *lhs, const void *rhs, void *ctx) {
    (void)ctx;
    const uint32_t a=*(const uint32_t *)lhs,b=*(const uint32_t *)rhs;
    return a>b ? -1 : a<b ? 1 : 0;
}

int main(void) {
    ranking_t *ranking = ranking_new();
    if (ranking == NULL) return EXIT_FAILURE;
    int64_t score = 0;
    size_t position = 0;
    int ok = ranking_upsert(ranking, "z", 12) == 0 &&
             ranking_upsert(ranking, "a", 12) == 0 &&
             ranking_get(ranking, "a", &score, &position) == 0 &&
             score == 12 && position == 0;
    ranking_destroy(ranking);
    if (!ok) return EXIT_FAILURE;

    /* Exercise the new installed generic composite ranking symbols. */
    ranking = ranking_new_custom(sizeof(uint32_t),external_key_compare,NULL);
    if (ranking == NULL) return EXIT_FAILURE;
    const uint32_t score_a=10U,score_b=20U;
    uint32_t selected=0U;
    ok = ranking_upsert_key(ranking,"a",&score_a)==0 &&
         ranking_upsert_key(ranking,"b",&score_b)==0 &&
         ranking_get_key(ranking,"b",&selected,&position)==0 &&
         selected==20U && position==0U &&
         ranking_at_key(ranking,1U,NULL,&selected)==0 && selected==10U;
    ranking_destroy(ranking);
    if (!ok) return EXIT_FAILURE;

    external_score_map_t map = {0};
    if (external_score_map_init(&map, hashmap_hash_string, strcmp) != 0)
        return EXIT_FAILURE;
    int points = 25;
    ok = external_score_map_put(&map, "participant", &points) == 0 &&
         external_score_map_get(&map, "participant") == &points;
    external_score_map_cleanup(&map);
    if (!ok) return EXIT_FAILURE;

    /* Test that the installed headers expose modern names and the exported
     * library provides the standards-compatible MT19937 entry points. */
    mtwister_t *rng = mtwister_new(UINT32_C(5489));
    if (rng == NULL) return EXIT_FAILURE;
    ok = mtwister_next_u32(rng) == UINT32_C(3499211612) &&
         mtwister_next_u32(rng) == UINT32_C(581869302);
    mtwister_destroy(rng);
    if (!ok) return EXIT_FAILURE;

    team_t *team = team_create("00000000-0000-4000-8000-000000000001",
                               "Brigate Ebosse", UINT32_C(24835));
    if (team == NULL) return EXIT_FAILURE;
    ok = strcmp(team->name,"Brigate Ebosse") == 0 &&
         team->total_points_tenths == UINT32_C(24835);
    team_destroy(team);
    if (!ok) return EXIT_FAILURE;

    const competition_team_input_t teams[2] = {
        {"00000000-0000-4000-8000-000000000001", "Brigate Ebosse"},
        {"00000000-0000-4000-8000-000000000002", "Boh"}
    };
    const competition_fixture_input_t match = {teams[0].uuid, teams[1].uuid};
    const competition_round_input_t round = {12U, &match, 1U};
    const competition_score_input_t scores[2] = {
        {"00000000-0000-4000-8000-000000000001", 12U,
         {745U,FANTASY_SCORE_FINAL}},
        {"00000000-0000-4000-8000-000000000002", 12U,
         {680U,FANTASY_SCORE_FINAL}}
    };
    const competition_input_t input = {teams,2U,&round,1U,scores,2U};
    competition_t *competition = NULL;
    if (competition_create(&input,&competition) != 0) return EXIT_FAILURE;
    uint32_t pair_count=0U;
    ok = competition_is_simulation_ready(competition) &&
         competition_pair_count(competition,0U,1U,&pair_count) == 0 &&
         pair_count == 1U;
    if (!ok) { competition_destroy(competition); return EXIT_FAILURE; }

    /* The installed library resolves the original championship without
     * private headers or calendar generation. */
    match_rules_t rules;
    match_result_t scored;
    season_resolution_t *season = NULL;
    ok = match_rules_default(&rules) == 0 &&
         match_resolve_scores(&rules, 745U, 680U, &scored) == 0 &&
         scored.home_goals == 3U && scored.away_goals == 1U &&
         season_resolve(competition, NULL, &rules,
                        SEASON_RESOLVE_FULL_SEASON, &season) == 0;
    if (!ok) {
        season_resolution_destroy(season);
        competition_destroy(competition);
        return EXIT_FAILURE;
    }
    season_rank_entry_t first;
    ok = season_resolution_rank_at(season, 0U, &first) == 0 &&
         first.standings.championship_points == 3U &&
         first.goal_stats.complete;
    season_resolution_destroy(season);
    if (!ok) { competition_destroy(competition); return EXIT_FAILURE; }

    /* The alternative AVL backend is available to installed consumers. */
    const season_resolution_options_t resolution_options = {
        SEASON_RESOLVE_FULL_SEASON,SEASON_RANKING_AVL
    };
    season = NULL;
    ok = season_resolve_with_options(competition,NULL,&rules,
                                     &resolution_options,&season)==0 &&
         season_resolution_rank_at(season,0U,&first)==0 &&
         first.standings.championship_points==3U;
    season_resolution_destroy(season);
    if (!ok) { competition_destroy(competition); return EXIT_FAILURE; }

    /* An installed consumer can generate and validate a calendar without
     * depending on private headers or the repository source tree. */
    calendar_options_t options = calendar_options_default();
    options.method = CALENDAR_METHOD_ROUND_SHUFFLE;
    calendar_generator_t *generator = NULL;
    calendar_t *permuted = NULL;
    if (calendar_generator_create(competition, &options, &generator) != 0 ||
        calendar_generator_next(generator, &permuted, NULL) != 0 ||
        calendar_validate(competition, permuted) != 0) {
        calendar_destroy(permuted);
        calendar_generator_destroy(generator);
        competition_destroy(competition);
        return EXIT_FAILURE;
    }
    calendar_generator_destroy(generator);
    calendar_destroy(permuted);

    /* Confirm installed-package availability of the 3.1.1 batch API. */
    monte_carlo_options_t mc_options = monte_carlo_options_default();
    mc_options.scope = SEASON_RESOLVE_FULL_SEASON;
    mc_options.requested_samples = 2U;
    mc_options.max_generation_attempts = 2U;
    monte_carlo_result_t *experiment = NULL;
    ok = monte_carlo_run(competition, &rules, &mc_options,
                         NULL, NULL, NULL, &experiment) == 0 &&
         monte_carlo_result_completion(experiment) == MONTE_CARLO_COMPLETED &&
         monte_carlo_result_accepted(experiment) == 2U &&
         monte_carlo_result_baseline(experiment) != NULL;
    monte_carlo_result_destroy(experiment);
    if (!ok) { competition_destroy(competition); return EXIT_FAILURE; }
    mc_options.ranking_backend=SEASON_RANKING_AVL;
    experiment=NULL;
    ok = monte_carlo_run(competition,&rules,&mc_options,
                         NULL,NULL,NULL,&experiment)==0 &&
         monte_carlo_result_accepted(experiment)==2U;
    monte_carlo_result_destroy(experiment);
    if (!ok) { competition_destroy(competition); return EXIT_FAILURE; }

    standings_t *standings = NULL;
    if (standings_create(competition, &standings) != 0) {
        competition_destroy(competition);
        return EXIT_FAILURE;
    }
    /* Standings own their UUID copy and can outlive the input snapshot. */
    competition_destroy(competition);
    ok = standings_record_match(standings, 0U, 1U, STANDINGS_WIN) == 0;
    standing_entry_t entry;
    ok = ok && standings_entry_for_uuid(standings, teams[0].uuid, &entry) == 0 &&
         entry.championship_points == 3U && entry.wins == 1U;
    standings_reset(standings);
    ok = ok && standings_entry_at(standings, 0U, &entry) == 0 &&
         entry.championship_points == 0U && entry.wins == 0U;
    standings_destroy(standings);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
