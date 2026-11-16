/*
 * LeaderboardSystem mutable championship standings (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "standings.h"
#include "alloc_internal.h"
#include "standings_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void standings_destroy(standings_t *s) {
    if (s == NULL) return;
    free(s->entries);
    free(s->ids);
    free(s);
}

int standings_create(const competition_t *competition, standings_t **out) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (competition == NULL) return -EINVAL;

    const size_t n = competition_team_count(competition);
    if (n == 0U || n > UINT32_MAX ||
        n > SIZE_MAX / sizeof(team_uuid_t) ||
        n > SIZE_MAX / sizeof(standing_entry_t)) return -EOVERFLOW;

    standings_t *s = leaderboard_alloc_calloc(1U, sizeof(*s));
    if (s == NULL) return -ENOMEM;
    s->team_count = n;
    s->ids = leaderboard_alloc_calloc(n, sizeof(*s->ids));
    s->entries = leaderboard_alloc_calloc(n, sizeof(*s->entries));
    if (s->ids == NULL || s->entries == NULL) {
        standings_destroy(s);
        return -ENOMEM;
    }
    for (size_t i = 0U; i < n; ++i) {
        const char *name = NULL;
        uint32_t fantasy_total = 0U;
        bool complete = false;
        /* The snapshot guarantees UUID-sorted indices and valid records. */
        int status = competition_team_at(competition, (team_index_t)i,
                                         &s->ids[i], &name, &fantasy_total,
                                         &complete);
        if (status != 0) {
            standings_destroy(s);
            return status;
        }
        s->entries[i].team_index = (team_index_t)i;
    }
    *out = s;
    return 0;
}

size_t standings_team_count(const standings_t *s) {
    return s == NULL ? 0U : s->team_count;
}

int standings_entry_at(const standings_t *s, team_index_t idx,
                       standing_entry_t *out) {
    if (s == NULL || out == NULL) return -EINVAL;
    if ((size_t)idx >= s->team_count) return -ERANGE;
    *out = s->entries[idx];
    return 0;
}

int standings_index_for_uuid(const standings_t *s, const char *uuid_text,
                             team_index_t *out) {
    team_uuid_t id;
    if (s == NULL || out == NULL || team_uuid_parse(uuid_text, &id) != 0)
        return -EINVAL;
    size_t lo = 0U, hi = s->team_count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2U;
        const int cmp = team_uuid_compare(&id, &s->ids[mid]);
        if (cmp == 0) { *out = (team_index_t)mid; return 0; }
        if (cmp < 0) hi = mid;
        else lo = mid + 1U;
    }
    return -ENOENT;
}

int standings_entry_for_uuid(const standings_t *s, const char *uuid_text,
                             standing_entry_t *out) {
    if (out == NULL) return -EINVAL;
    team_index_t idx;
    int status = standings_index_for_uuid(s, uuid_text, &idx);
    return status != 0 ? status : standings_entry_at(s, idx, out);
}

void standings_reset(standings_t *s) {
    if (s == NULL) return;
    memset(s->entries, 0, s->team_count * sizeof(*s->entries));
    for (size_t i = 0U; i < s->team_count; ++i)
        s->entries[i].team_index = (team_index_t)i;
}

/* Compute into a temporary copy, leaving the caller's state unchanged when
 * the update cannot be represented in uint32_t. */
static int updated_entry(const standing_entry_t *old,
                         standings_result_t result, standing_entry_t *next) {
    if (result != STANDINGS_WIN && result != STANDINGS_DRAW &&
        result != STANDINGS_LOSS) return -EINVAL;

    *next = *old;
    switch (result) {
        case STANDINGS_WIN:
            if (next->wins == UINT32_MAX ||
                next->championship_points > UINT32_MAX - 3U)
                return -EOVERFLOW;
            next->wins++;
            next->championship_points += 3U;
            break;
        case STANDINGS_DRAW:
            if (next->draws == UINT32_MAX ||
                next->championship_points == UINT32_MAX)
                return -EOVERFLOW;
            next->draws++;
            next->championship_points++;
            break;
        case STANDINGS_LOSS:
            if (next->losses == UINT32_MAX) return -EOVERFLOW;
            next->losses++;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

int standings_record_result(standings_t *s, team_index_t idx,
                            standings_result_t result) {
    if (s == NULL) return -EINVAL;
    if ((size_t)idx >= s->team_count) return -ERANGE;
    standing_entry_t next;
    int status = updated_entry(&s->entries[idx], result, &next);
    if (status != 0) return status;
    s->entries[idx] = next;
    return 0;
}

int standings_record_match(standings_t *s, team_index_t home,
                           team_index_t away, standings_result_t home_result) {
    if (s == NULL || home == away) return -EINVAL;
    if ((size_t)home >= s->team_count || (size_t)away >= s->team_count)
        return -ERANGE;
    if (home_result != STANDINGS_WIN && home_result != STANDINGS_DRAW &&
        home_result != STANDINGS_LOSS) return -EINVAL;

    const standings_result_t away_result =
        home_result == STANDINGS_WIN ? STANDINGS_LOSS :
        home_result == STANDINGS_LOSS ? STANDINGS_WIN : STANDINGS_DRAW;
    standing_entry_t home_next, away_next;
    int status = updated_entry(&s->entries[home], home_result, &home_next);
    if (status != 0) return status;
    status = updated_entry(&s->entries[away], away_result, &away_next);
    if (status != 0) return status;
    s->entries[home] = home_next;
    s->entries[away] = away_next;
    return 0;
}
