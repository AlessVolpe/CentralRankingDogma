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
#include "hashmap_typed.h"

HASHMAP_TYPED_DEFINE(external_score_map, char, int)

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
