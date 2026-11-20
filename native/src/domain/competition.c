/*
 * LeaderboardSystem immutable competition snapshot (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "competition.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct competition {
    team_t *teams;                    /* team_count, sorted by binary UUID */
    size_t team_count;
    uint32_t *serie_a_matchdays;     /* round_count, strictly increasing */
    size_t round_count;
    fixture_t *fixtures;              /* round-major, n / 2 per round */
    fantasy_score_t *scores;          /* round-major, n per round */
    uint32_t *pair_counts;            /* symmetric n x n, diagonal zero */
    bool ready;
};

static int size_multiply(size_t a, size_t b, size_t *out) {
    if (out == NULL || (a != 0U && b > SIZE_MAX / a)) return -EOVERFLOW;
    *out = a * b;
    return 0;
}

static int sort_team_by_uuid(const void *lhs, const void *rhs) {
    const team_t *a = lhs;
    const team_t *b = rhs;
    return team_uuid_compare(&a->id, &b->id);
}

static int team_index_by_uuid(const competition_t *c,
                              const team_uuid_t *uuid, team_index_t *out) {
    size_t lo = 0U, hi = c->team_count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2U;
        const int cmp = team_uuid_compare(uuid, &c->teams[mid].id);
        if (cmp == 0) { *out = (team_index_t)mid; return 0; }
        if (cmp < 0) hi = mid;
        else lo = mid + 1U;
    }
    return -ENOENT;
}

static int input_team_index(const competition_t *c,
                            const char *uuid_text, team_index_t *out) {
    team_uuid_t uuid;
    if (team_uuid_parse(uuid_text, &uuid) != 0) return -EINVAL;
    return team_index_by_uuid(c, &uuid, out) == 0 ? 0 : -EINVAL;
}

static int find_round(const competition_t *c, uint32_t matchday, size_t *out) {
    size_t low = 0U, high = c->round_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2U;
        if (c->serie_a_matchdays[mid] < matchday) low = mid + 1U;
        else high = mid;
    }
    if (low >= c->round_count || c->serie_a_matchdays[low] != matchday)
        return -EINVAL;
    *out = low;
    return 0;
}

/* Owned team_t is created via team_create but stored densely by value. */
static void release_teams(competition_t *c) {
    if (c->teams == NULL) return;
    for (size_t i = 0U; i < c->team_count; ++i) free(c->teams[i].name);
    free(c->teams);
    c->teams = NULL;
}

void competition_destroy(competition_t *c) {
    if (c == NULL) return;
    release_teams(c);
    free(c->serie_a_matchdays);
    free(c->fixtures);
    free(c->scores);
    free(c->pair_counts);
    free(c);
}

int competition_create(const competition_input_t *input, competition_t **out) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (input == NULL) return -EINVAL;
    const size_t n = input->team_count, r = input->round_count;
    if (n < 2U || (n & 1U) != 0U || n > UINT32_MAX || r == 0U ||
        n > SIZE_MAX / sizeof(team_t) ||
        input->teams == NULL || input->rounds == NULL || input->scores == NULL)
        return -EINVAL;

    size_t nr, nfixtures, nn;
    if (size_multiply(n, r, &nr) != 0 ||
        size_multiply(r, n / 2U, &nfixtures) != 0 ||
        size_multiply(n, n, &nn) != 0 ||
        nr > SIZE_MAX / sizeof(fantasy_score_t) ||
        nr > SIZE_MAX / sizeof(unsigned char) ||
        nfixtures > SIZE_MAX / sizeof(fixture_t) ||
        nn > SIZE_MAX / sizeof(uint32_t) ||
        r > SIZE_MAX / sizeof(uint32_t)) return -EOVERFLOW;
    if (input->score_count != nr) return -EINVAL;

    competition_t *c = leaderboard_alloc_calloc(1U, sizeof(*c));
    if (c == NULL) return -ENOMEM;
    c->team_count = n;
    c->round_count = r;
    c->ready = true;
    c->teams = leaderboard_alloc_calloc(n, sizeof(*c->teams));
    c->serie_a_matchdays = leaderboard_alloc_malloc(r * sizeof(*c->serie_a_matchdays));
    c->fixtures = leaderboard_alloc_malloc(nfixtures * sizeof(*c->fixtures));
    c->scores = leaderboard_alloc_calloc(nr, sizeof(*c->scores));
    c->pair_counts = leaderboard_alloc_calloc(nn, sizeof(*c->pair_counts));
    unsigned char *seen = leaderboard_alloc_calloc(n, sizeof(*seen));
    unsigned char *score_seen = leaderboard_alloc_calloc(nr, sizeof(*score_seen));
    if (c->teams == NULL || c->serie_a_matchdays == NULL ||
        c->fixtures == NULL || c->scores == NULL || c->pair_counts == NULL ||
        seen == NULL || score_seen == NULL) {
        free(seen); free(score_seen); competition_destroy(c); return -ENOMEM;
    }

    int error = 0;
    for (size_t i = 0U; i < n; ++i) {
        const competition_team_input_t *src = &input->teams[i];
        team_uuid_t parsed;
        if (team_uuid_parse(src->uuid, &parsed) != 0) { error = -EINVAL; goto fail; }
        if (src->name == NULL || src->name[0] == '\0') { error = -EINVAL; goto fail; }
        size_t length = 0U;
        while (length <= TEAM_NAME_MAX_BYTES && src->name[length] != '\0') ++length;
        if (length > TEAM_NAME_MAX_BYTES) { error = -EINVAL; goto fail; }
        /* Canonical name allocation uses the team constructor. */
        team_t *created = team_create(src->uuid, src->name, 0U);
        if (created == NULL) { error = -ENOMEM; goto fail; }
        c->teams[i] = *created;
        free(created); /* transfer name ownership to dense array */
    }
    qsort(c->teams, n, sizeof(*c->teams), sort_team_by_uuid);
    for (size_t i = 1U; i < n; ++i) {
        if (team_uuid_compare(&c->teams[i-1U].id, &c->teams[i].id) == 0) {
            error = -EEXIST; goto fail;
        }
    }

    for (size_t ri = 0U; ri < r; ++ri) {
        const competition_round_input_t *src = &input->rounds[ri];
        if (src->serie_a_matchday == 0U ||
            (ri != 0U && src->serie_a_matchday <= c->serie_a_matchdays[ri-1U]) ||
            src->fixtures == NULL || src->fixture_count != n / 2U) {
            error = -EINVAL; goto fail;
        }
        c->serie_a_matchdays[ri] = src->serie_a_matchday;
        memset(seen, 0, n);
        for (size_t fi = 0U; fi < n / 2U; ++fi) {
            team_index_t home, away;
            if (input_team_index(c, src->fixtures[fi].home_uuid, &home) != 0 ||
                input_team_index(c, src->fixtures[fi].away_uuid, &away) != 0 ||
                home == away || seen[home] || seen[away]) {
                error = -EINVAL; goto fail;
            }
            seen[home] = 1U;
            seen[away] = 1U;
            c->fixtures[ri*(n/2U)+fi] = (fixture_t){home, away};
            const size_t left = (size_t)home * n + away;
            const size_t right = (size_t)away * n + home;
            if (c->pair_counts[left] == UINT32_MAX) {
                error = -EOVERFLOW; goto fail;
            }
            c->pair_counts[left]++;
            c->pair_counts[right]++;
        }
    }

    for (size_t i = 0U; i < nr; ++i) {
        const competition_score_input_t *src = &input->scores[i];
        team_index_t ti;
        size_t ri;
        if (input_team_index(c, src->team_uuid, &ti) != 0 ||
            find_round(c, src->serie_a_matchday, &ri) != 0 ||
            (src->score.status != FANTASY_SCORE_PENDING &&
             src->score.status != FANTASY_SCORE_FINAL) ||
            (src->score.status == FANTASY_SCORE_PENDING && src->score.points_tenths != 0U)) {
            error = -EINVAL; goto fail;
        }
        size_t idx = ri*n+ti;
        if (score_seen[idx]) { error = -EEXIST; goto fail; }
        score_seen[idx] = 1U;
        c->scores[idx] = src->score;
        if (src->score.status == FANTASY_SCORE_FINAL) {
            uint32_t total = c->teams[ti].total_points_tenths;
            if (UINT32_MAX - total < src->score.points_tenths) {
                error = -EOVERFLOW; goto fail;
            }
            c->teams[ti].total_points_tenths = total + src->score.points_tenths;
        } else c->ready = false;
    }
    for (size_t i = 0U; i < nr; ++i) {
        if (!score_seen[i]) { error = -EINVAL; goto fail; }
    }
    free(seen); free(score_seen);
    *out = c;
    return 0;

fail:
    free(seen); free(score_seen);
    competition_destroy(c);
    return error;
}

size_t competition_team_count(const competition_t *c) {
    return c == NULL ? 0U : c->team_count;
}
size_t competition_round_count(const competition_t *c) {
    return c == NULL ? 0U : c->round_count;
}

int competition_team_at(const competition_t *c, team_index_t index,
                        team_uuid_t *uuid, const char **name,
                        uint32_t *total, bool *complete) {
    if (c == NULL || uuid == NULL || name == NULL || total == NULL || complete == NULL)
        return -EINVAL;
    if (index >= c->team_count) return -ERANGE;
    *uuid = c->teams[index].id;
    *name = c->teams[index].name;
    *total = c->teams[index].total_points_tenths;
    bool team_complete = true;
    for (size_t ri=0U; ri<c->round_count; ++ri) {
        if (c->scores[ri*c->team_count + index].status != FANTASY_SCORE_FINAL)
            team_complete = false;
    }
    *complete = team_complete;
    return 0;
}

int competition_team_index(const competition_t *c,
                           const char *uuid_text, team_index_t *out_index) {
    team_uuid_t uuid;
    if (c == NULL || out_index == NULL || team_uuid_parse(uuid_text, &uuid) != 0)
        return -EINVAL;
    return team_index_by_uuid(c, &uuid, out_index);
}

int competition_round_matchday(const competition_t *c, size_t index,
                               uint32_t *out_matchday) {
    if (c == NULL || out_matchday == NULL) return -EINVAL;
    if (index >= c->round_count) return -ERANGE;
    *out_matchday = c->serie_a_matchdays[index];
    return 0;
}

int competition_fixture_at(const competition_t *c, size_t ri, size_t fi,
                           fixture_t *out) {
    if (c == NULL || out == NULL) return -EINVAL;
    if (ri >= c->round_count || fi >= c->team_count/2U) return -ERANGE;
    *out = c->fixtures[ri*(c->team_count/2U)+fi];
    return 0;
}

int competition_score_at(const competition_t *c, team_index_t ti,
                         size_t ri, fantasy_score_t *out) {
    if (c == NULL || out == NULL) return -EINVAL;
    if (ti >= c->team_count || ri >= c->round_count) return -ERANGE;
    *out = c->scores[ri*c->team_count+ti];
    return 0;
}

int competition_pair_count(const competition_t *c, team_index_t a,
                           team_index_t b, uint32_t *out) {
    if (c == NULL || out == NULL) return -EINVAL;
    if (a >= c->team_count || b >= c->team_count || a == b) return -ERANGE;
    *out = c->pair_counts[(size_t)a*c->team_count+b];
    return 0;
}

bool competition_is_simulation_ready(const competition_t *c) {
    return c != NULL && c->ready;
}
