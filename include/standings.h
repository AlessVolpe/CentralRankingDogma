/*
 * LeaderboardSystem mutable championship standings (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_STANDINGS_H
#define LEADERBOARD_STANDINGS_H

#include <stddef.h>
#include <stdint.h>
#include "competition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This result refers to a single team's outcome, not fantasy-point totals. */
typedef enum standings_result {
    STANDINGS_LOSS = 0,
    STANDINGS_DRAW = 1,
    STANDINGS_WIN = 3
} standings_result_t;

typedef struct standing_entry {
    team_index_t team_index;
    uint32_t championship_points; /* exactly 3 * wins + draws */
    uint32_t wins;
    uint32_t draws;
    uint32_t losses;
} standing_entry_t;

typedef struct standings standings_t;

/* Creates zeroed standings with the same UUID-sorted indices as competition.
 * Copies the team UUIDs; the source competition can be destroyed afterwards.
 * Accepts partially completed competition snapshots. All allocations are
 * owned by standings_t. Returns 0 or negative errno; *out=NULL on failure. */
int standings_create(const competition_t *competition, standings_t **out);
void standings_destroy(standings_t *standings);
size_t standings_team_count(const standings_t *standings);

/* A successful lookup copies the entry into caller memory. No pointer into
 * mutable internal state escapes. Invalid input: -EINVAL; unknown UUID:
 * -ENOENT; out-of-bounds team index: -ERANGE. */
int standings_entry_at(const standings_t *standings, team_index_t team_index,
                       standing_entry_t *out);
int standings_index_for_uuid(const standings_t *standings,
                             const char *uuid_text, team_index_t *out_index);
int standings_entry_for_uuid(const standings_t *standings,
                             const char *uuid_text, standing_entry_t *out);

/* Reuse a collection for the next calendar simulation, leaving its team
 * identities intact. NULL is allowed. */
void standings_reset(standings_t *standings);

/* Increment one team's match result. No opponent relationship is inferred;
 * prefer standings_record_match() for a two-team result. Successful updates
 * keep championship_points == 3*wins + draws. Overflow leaves state intact.
 * Invalid input/result: -EINVAL; invalid index: -ERANGE; overflow: -EOVERFLOW. */
int standings_record_result(standings_t *standings, team_index_t team_index,
                            standings_result_t result);

/* Apply the outcome to BOTH teams atomically. home_result is the home team's
 * outcome; the away result is inferred (win/loss inverted, draw/draw).
 * "Home" is an identifier for a fixture's first team, NOT a points bonus.
 * Both entries are unchanged on any error, including arithmetic overflow.
 * Rejects self matches and invalid result enums with -EINVAL. */
int standings_record_match(standings_t *standings,
                           team_index_t home, team_index_t away,
                           standings_result_t home_result);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_STANDINGS_H */
