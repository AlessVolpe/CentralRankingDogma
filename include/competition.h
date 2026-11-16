/*
 * LeaderboardSystem immutable competition snapshot (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_COMPETITION_H
#define LEADERBOARD_COMPETITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "team.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t team_index_t;

typedef struct fixture {
    team_index_t home;
    team_index_t away;
} fixture_t;

typedef struct competition_team_input {
    const char *uuid;        /* text UUID, case-insensitive */
    const char *name;        /* team display name; duplicates allowed */
} competition_team_input_t;

typedef struct competition_fixture_input {
    const char *home_uuid;
    const char *away_uuid;
} competition_fixture_input_t;

typedef struct competition_round_input {
    uint32_t serie_a_matchday; /* starts at 1; strictly increasing */
    const competition_fixture_input_t *fixtures;
    size_t fixture_count;      /* exactly team_count / 2 */
} competition_round_input_t;

typedef enum fantasy_score_status {
    FANTASY_SCORE_PENDING = 0,
    FANTASY_SCORE_FINAL = 1
} fantasy_score_status_t;

typedef struct fantasy_score {
    uint32_t points_tenths;
    fantasy_score_status_t status;
} fantasy_score_t;

typedef struct competition_score_input {
    const char *team_uuid;
    uint32_t serie_a_matchday;
    fantasy_score_t score; /* pending => points_tenths must be 0 */
} competition_score_input_t;

typedef struct competition_input {
    const competition_team_input_t *teams;
    size_t team_count;  /* even, >=2 */
    const competition_round_input_t *rounds;
    size_t round_count; /* >=1, strict increasing mapped matchdays */
    const competition_score_input_t *scores;
    size_t score_count; /* EXACTLY team_count * round_count, including PENDING */
} competition_input_t;

typedef struct competition competition_t;

/* 0 success; -EINVAL invalid/dangling/missing input; -EEXIST duplicate UUID,
 * score, or matchday; -EOVERFLOW checked arithmetic overflow; -ENOMEM
 * allocation failure. On ANY failure, *out = NULL. No database dependency. */
int competition_create(const competition_input_t *input, competition_t **out);
void competition_destroy(competition_t *competition);
size_t competition_team_count(const competition_t *competition);
size_t competition_round_count(const competition_t *competition);

/* Copies UUID and cumulative FINAL fantasy total; *complete is true iff
 * this team's scores are FINAL for every imported competition matchday.
 * Name is a borrowed read-only pointer valid until competition_destroy().
 * 0 success; -EINVAL invalid argument; -ERANGE invalid index. */
int competition_team_at(const competition_t *competition, team_index_t index,
                        team_uuid_t *uuid, const char **name,
                        uint32_t *final_total_tenths, bool *complete);
int competition_team_index(const competition_t *competition,
                           const char *uuid_text, team_index_t *out_index);
int competition_round_matchday(const competition_t *competition,
                               size_t round_index, uint32_t *out_matchday);
int competition_fixture_at(const competition_t *competition,
                           size_t round_index, size_t fixture_index,
                           fixture_t *out_fixture);
int competition_score_at(const competition_t *competition,
                         team_index_t team_index, size_t round_index,
                         fantasy_score_t *out_score);
int competition_pair_count(const competition_t *competition,
                           team_index_t a, team_index_t b, uint32_t *out_count);
/* A complete season has no pending scores and can be simulated in 3.1.0. */
bool competition_is_simulation_ready(const competition_t *competition);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_COMPETITION_H */
