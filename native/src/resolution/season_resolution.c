/*
 * LeaderboardSystem deterministic season evaluation (milestone 3.1.0).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "season_resolution.h"
#include "ranking.h"
#include "alloc_internal.h"
#include "research_profile_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct season_resolution {
    size_t round_count;
    size_t fixture_count;
    size_t team_count;
    standings_t *standings;
    season_fixture_result_t *fixtures;
    season_rank_entry_t *ranks;
    team_uuid_t *ids;
};

void season_resolution_destroy(season_resolution_t *r) {
    if (r == NULL) return;
    standings_destroy(r->standings);
    free(r->fixtures);
    free(r->ranks);
    free(r->ids);
    free(r);
}

static int rank_cmp(const void *lhs, const void *rhs) {
    const season_rank_entry_t *a = lhs, *b = rhs;
    if (a->standings.championship_points != b->standings.championship_points)
        return a->standings.championship_points > b->standings.championship_points ? -1 : 1;
    if (a->fantasy_total_tenths != b->fantasy_total_tenths)
        return a->fantasy_total_tenths > b->fantasy_total_tenths ? -1 : 1;
    if (a->goal_difference != b->goal_difference)
        return a->goal_difference > b->goal_difference ? -1 : 1;
    /* competition indices follow sorted binary UUIDs, not input ordering. */
    if (a->team_index < b->team_index) return -1;
    if (a->team_index > b->team_index) return 1;
    return 0;
}

/* In-place heapsort with a worst-case O(N log N) upper bound. A portable
 * C library need not guarantee an asymptotic bound for libc qsort(). */
static void swap_ranks(season_rank_entry_t *a, season_rank_entry_t *b) {
    season_rank_entry_t tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Rank comparator is negative for the earlier sporting position. Build a
 * max-heap of the LATER positions so extraction produces the desired order. */
static void sift_down(season_rank_entry_t *r, size_t root, size_t end) {
    while (root < end / 2U) {
        size_t child = root * 2U + 1U;
        size_t late = root;
        if (rank_cmp(&r[late], &r[child]) < 0) late = child;
        if (child + 1U < end && rank_cmp(&r[late], &r[child + 1U]) < 0)
            late = child + 1U;
        if (late == root) return;
        swap_ranks(&r[root], &r[late]);
        root = late;
    }
}

static void sort_ranks(season_rank_entry_t *r, size_t n) {
    for (size_t i = n / 2U; i > 0U; --i) sift_down(r, i - 1U, n);
    for (size_t end = n; end > 1U; --end) {
        swap_ranks(&r[0], &r[end - 1U]);
        sift_down(r, 0U, end - 1U);
    }
}

/* A domain-specific key; unlike the generic ranking's ID tiebreak, equality
 * here means a genuine sporting tie. Never pack into one signed integer. */
typedef struct sporting_order_key {
    uint32_t points;
    uint32_t fantasy_tenths;
    int64_t goal_difference;
} sporting_order_key_t;

static int sporting_key_compare(const void *lhs, const void *rhs, void *unused) {
    (void)unused;
    const sporting_order_key_t *a = lhs;
    const sporting_order_key_t *b = rhs;
    if (a->points != b->points) return a->points > b->points ? -1 : 1;
    if (a->fantasy_tenths != b->fantasy_tenths)
        return a->fantasy_tenths > b->fantasy_tenths ? -1 : 1;
    if (a->goal_difference != b->goal_difference)
        return a->goal_difference > b->goal_difference ? -1 : 1;
    return 0;
}

/* The original team index is UUID-sorted. Its fixed-width hexadecimal text
 * is consequently a portable ID-order tiebreak that exactly matches the
 * reference heapsort, without relying on host byte order or UUID padding. */
static int sort_ranks_avl(season_rank_entry_t *ranks, size_t n) {
    ranking_t *tree = ranking_new_custom(sizeof(sporting_order_key_t),
                                        sporting_key_compare, NULL);
    if (tree == NULL) return -ENOMEM;
    int status = 0;
    for (size_t i = 0U; i < n; ++i) {
        const sporting_order_key_t key = {
            ranks[i].standings.championship_points,
            ranks[i].fantasy_total_tenths,
            ranks[i].goal_difference
        };
        char id[9];
        (void)snprintf(id, sizeof(id), "%08" PRIx32, (uint32_t)ranks[i].team_index);
        status = ranking_upsert_key(tree, id, &key);
        if (status != 0) goto done;
    }
    season_rank_entry_t *sorted = leaderboard_alloc_malloc(n * sizeof(*sorted));
    if (sorted == NULL) { status = -ENOMEM; goto done; }
    for (size_t pos = 0U; pos < n; ++pos) {
        const char *id = NULL;
        status = ranking_at_key(tree, pos, &id, NULL);
        if (status != 0) break;
        const unsigned long index = strtoul(id, NULL, 16);
        if (index >= n) { status = -EINVAL; break; }
        sorted[pos] = ranks[index];
    }
    if (status == 0) memcpy(ranks, sorted, n * sizeof(*sorted));
    free(sorted);
done:
    ranking_destroy(tree);
    return status;
}

static bool sporting_tie(const season_rank_entry_t *a,
                         const season_rank_entry_t *b) {
    return a->standings.championship_points == b->standings.championship_points &&
           a->fantasy_total_tenths == b->fantasy_total_tenths &&
           a->goal_difference == b->goal_difference;
}

static int finalized_prefix(const competition_t *c, size_t n, size_t r,
                            season_resolution_scope_t scope, size_t *out) {
    if (scope == SEASON_RESOLVE_FULL_SEASON &&
        !competition_is_simulation_ready(c)) return -EAGAIN;
    if (scope != SEASON_RESOLVE_FULL_SEASON &&
        scope != SEASON_RESOLVE_FINALIZED_PREFIX) return -EINVAL;
    for (size_t round = 0U; round < r; ++round) {
        for (size_t team = 0U; team < n; ++team) {
            fantasy_score_t score;
            int status = competition_score_at(c, (team_index_t)team, round, &score);
            if (status != 0) return status;
            if (score.status != FANTASY_SCORE_FINAL) {
                if (scope == SEASON_RESOLVE_FULL_SEASON) return -EAGAIN;
                *out = round;
                return 0;
            }
        }
    }
    *out = r;
    return 0;
}

int season_resolve_with_options(const competition_t *competition,
                                const calendar_t *selected_calendar,
                                const match_rules_t *rules,
                                const season_resolution_options_t *options,
                                season_resolution_t **out) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (competition == NULL || match_rules_validate(rules) != 0 ||
        options == NULL ||
        (options->scope != SEASON_RESOLVE_FULL_SEASON &&
         options->scope != SEASON_RESOLVE_FINALIZED_PREFIX) ||
        (options->ranking_backend != SEASON_RANKING_HEAPSORT &&
         options->ranking_backend != SEASON_RANKING_AVL)) return -EINVAL;
    const size_t n = competition_team_count(competition);
    const size_t r = competition_round_count(competition);
    if (n == 0U || n > UINT32_MAX || (n & 1U) != 0U ||
        n > SIZE_MAX / sizeof(team_uuid_t) ||
        n > SIZE_MAX / sizeof(season_rank_entry_t)) return -EOVERFLOW;
    /* The final (possibly partial) N-1-round scheduling cycle starts here. */
    const size_t final_cycle_start = ((r - 1U) / (n - 1U)) * (n - 1U);
    if (selected_calendar != NULL) {
        int status = calendar_validate(competition, selected_calendar);
        if (status != 0) return status;
    }
    size_t rounds = 0U;
    int status = finalized_prefix(competition, n, r, options->scope, &rounds);
    if (status != 0) return status;
    const size_t fixtures_per_round = n / 2U;
    if (rounds > SIZE_MAX / fixtures_per_round) return -EOVERFLOW;
    const size_t count = rounds * fixtures_per_round;
    if (count > SIZE_MAX / sizeof(season_fixture_result_t)) return -EOVERFLOW;
    season_resolution_t *result = leaderboard_alloc_calloc(1U, sizeof(*result));
    if (result == NULL) return -ENOMEM;
    result->team_count = n;
    result->round_count = rounds;
    result->fixture_count = count;
    result->ids = leaderboard_alloc_calloc(n, sizeof(*result->ids));
    result->ranks = leaderboard_alloc_calloc(n, sizeof(*result->ranks));
    if (count != 0U)
        result->fixtures = leaderboard_alloc_calloc(count, sizeof(*result->fixtures));
    if (result->ids == NULL || result->ranks == NULL ||
        (count != 0U && result->fixtures == NULL)) {
        status = -ENOMEM;
        goto fail;
    }
    status = standings_create(competition, &result->standings);
    if (status != 0) goto fail;

    for (size_t ti = 0U; ti < n; ++ti) {
        const char *name = NULL;
        uint32_t total = 0U;
        bool complete = false;
        status = competition_team_at(competition, (team_index_t)ti,
                                     &result->ids[ti], &name, &total, &complete);
        if (status != 0) goto fail;
        result->ranks[ti].team_index = (team_index_t)ti;
    }
    for (size_t ri = 0U; ri < rounds; ++ri) {
        uint32_t day = 0U;
        status = competition_round_matchday(competition, ri, &day);
        if (status != 0) goto fail;
        /* Every score is accumulated once per round, irrespective of the
         * selected opponents. This makes the tie-break invariant explicit. */
        for (size_t ti = 0U; ti < n; ++ti) {
            fantasy_score_t score;
            status = competition_score_at(competition, (team_index_t)ti, ri, &score);
            if (status != 0) goto fail;
            if (score.status != FANTASY_SCORE_FINAL) { status = -EINVAL; goto fail; }
            uint32_t *total = &result->ranks[ti].fantasy_total_tenths;
            if (score.points_tenths > UINT32_MAX - *total) {
                status = -EOVERFLOW;
                goto fail;
            }
            *total += score.points_tenths;
        }
        for (size_t fi = 0U; fi < fixtures_per_round; ++fi) {
            fixture_t f;
            status = selected_calendar == NULL ?
                competition_fixture_at(competition, ri, fi, &f) :
                calendar_fixture_at(selected_calendar, ri, fi, &f);
            if (status != 0) goto fail;
            if (f.home >= n || f.away >= n || f.home == f.away) {
                status = -EINVAL;
                goto fail;
            }
            fantasy_score_t home, away;
            status = competition_score_at(competition, f.home, ri, &home);
            if (status != 0) goto fail;
            status = competition_score_at(competition, f.away, ri, &away);
            if (status != 0) goto fail;
            if (home.status != FANTASY_SCORE_FINAL ||
                away.status != FANTASY_SCORE_FINAL) { status = -EINVAL; goto fail; }
            season_fixture_result_t *record =
                &result->fixtures[ri * fixtures_per_round + fi];
            match_rules_t round_rules = *rules;
            if (round_rules.home_advantage_exclude_final_cycle &&
                ri >= final_cycle_start)
                round_rules.home_advantage_enabled = false;
            status = match_resolve_scores(&round_rules, home.points_tenths,
                                         away.points_tenths, &record->result);
            if (status != 0) goto fail;
            status = standings_record_scored_match(result->standings,
                      f.home, f.away, record->result.home_goals,
                      record->result.away_goals);
            if (status != 0) goto fail;
            record->fixture = f;
            record->round_index = ri;
            record->fixture_index = fi;
            record->serie_a_matchday = day;
        }
    }
    for (size_t ti = 0U; ti < n; ++ti) {
        season_rank_entry_t *entry = &result->ranks[ti];
        status = standings_entry_at(result->standings, (team_index_t)ti,
                                    &entry->standings);
        if (status != 0) goto fail;
        status = standings_goal_stats_at(result->standings, (team_index_t)ti,
                                         &entry->goal_stats);
        if (status != 0) goto fail;
        if (!entry->goal_stats.complete) { status = -EINVAL; goto fail; }
        entry->goal_difference = (int64_t)entry->goal_stats.goals_for -
                                 (int64_t)entry->goal_stats.goals_against;
    }
    LB_PROFILE_BEGIN(LEADERBOARD_PROFILE_RANKING);
    if (options->ranking_backend == SEASON_RANKING_AVL) {
        status = sort_ranks_avl(result->ranks, n);
        if (status != 0) {
            LB_PROFILE_END(LEADERBOARD_PROFILE_RANKING);
            goto fail;
        }
    } else {
        sort_ranks(result->ranks, n);
    }
    for (size_t i = 0U; i < n; ++i) {
        if (i > 0U && sporting_tie(&result->ranks[i-1U], &result->ranks[i]))
            result->ranks[i].sporting_rank = result->ranks[i-1U].sporting_rank;
        else result->ranks[i].sporting_rank = (uint32_t)(i + 1U);
    }
    LB_PROFILE_END(LEADERBOARD_PROFILE_RANKING);
    *out = result;
    return 0;
fail:
    season_resolution_destroy(result);
    return status;
}

int season_resolve(const competition_t *competition,
                   const calendar_t *selected_calendar,
                   const match_rules_t *rules,
                   season_resolution_scope_t scope,
                   season_resolution_t **out) {
    const season_resolution_options_t options = {
        scope, SEASON_RANKING_HEAPSORT
    };
    return season_resolve_with_options(competition, selected_calendar,
                                       rules, &options, out);
}

size_t season_resolution_round_count(const season_resolution_t *r) {
    return r == NULL ? 0U : r->round_count;
}
size_t season_resolution_fixture_count(const season_resolution_t *r) {
    return r == NULL ? 0U : r->fixture_count;
}
size_t season_resolution_team_count(const season_resolution_t *r) {
    return r == NULL ? 0U : r->team_count;
}
int season_resolution_fixture_at(const season_resolution_t *r, size_t index,
                                 season_fixture_result_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    if (index >= r->fixture_count) return -ERANGE;
    *out = r->fixtures[index];
    return 0;
}
int season_resolution_rank_at(const season_resolution_t *r, size_t position,
                              season_rank_entry_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    if (position >= r->team_count) return -ERANGE;
    *out = r->ranks[position];
    return 0;
}
int season_resolution_team_uuid_at(const season_resolution_t *r,
                                   team_index_t index, team_uuid_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    if (index >= r->team_count) return -ERANGE;
    *out = r->ids[index];
    return 0;
}
