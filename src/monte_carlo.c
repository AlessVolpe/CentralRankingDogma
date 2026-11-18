/*
 * LeaderboardSystem reproducible calendar experiments (milestone 3.1.1).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "monte_carlo.h"
#include "alloc_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct monte_carlo_result {
    monte_carlo_completion_t completion;
    monte_carlo_options_t options; /* callback-free reproducibility metadata */
    size_t attempted, accepted, unique, duplicates, rounds, teams;
    calendar_diagnostics_t generation; /* all successful draws, even rejected */
    season_resolution_t *baseline;
    monte_carlo_team_stats_t *stats; /* indexed by source team index */
    double *points_m2;             /* Welford second central moment */
    size_t *positions;             /* [team][sporting_rank - 1] */
};

typedef struct seen_node {
    struct seen_node *next;
    uint64_t hash;
    fixture_t fixtures[];
} seen_node_t;

typedef struct seen_table {
    seen_node_t **buckets;
    size_t bucket_count, count, bytes_used, limit, fixture_bytes;
} seen_table_t;

static int checked_multiply(size_t a, size_t b, size_t *out) {
    if (a != 0U && b > SIZE_MAX / a) return -EOVERFLOW;
    *out = a * b;
    return 0;
}

/* The diagnostic counters can be large even when accepted samples are few. */
static int accumulate_generation(calendar_diagnostics_t *sum,
                                 const calendar_diagnostics_t *draw) {
#define ACCUM_DIAG(field, maximum) do { \
    if (draw->field > (maximum) - sum->field) return -EOVERFLOW; \
    sum->field += draw->field; \
} while (0)
    ACCUM_DIAG(two_round_attempts, UINT64_MAX);
    ACCUM_DIAG(two_round_partial_moves, UINT64_MAX);
    ACCUM_DIAG(two_round_whole_swaps, UINT64_MAX);
    ACCUM_DIAG(three_round_attempts, UINT64_MAX);
    ACCUM_DIAG(three_round_moves, UINT64_MAX);
    ACCUM_DIAG(three_round_search_exhaustions, UINT64_MAX);
    ACCUM_DIAG(three_round_nodes, UINT64_MAX);
    ACCUM_DIAG(changed_base_rounds, SIZE_MAX);
    ACCUM_DIAG(changed_season_rounds, SIZE_MAX);
#undef ACCUM_DIAG
    return 0;
}

static void seen_destroy(seen_table_t *seen) {
    if (seen->buckets != NULL) {
        for (size_t i = 0; i < seen->bucket_count; ++i) {
            seen_node_t *p = seen->buckets[i];
            while (p != NULL) {
                seen_node_t *next = p->next;
                free(p);
                p = next;
            }
        }
    }
    free(seen->buckets);
    memset(seen, 0, sizeof(*seen));
}

/* Return 1 for a user-configured uniqueness-memory limit; negative errno
 * for real failures. Account both tables during rehash (peak live storage). */
static int seen_resize(seen_table_t *seen, size_t next_count) {
    size_t bytes;
    if (checked_multiply(next_count, sizeof(*seen->buckets), &bytes) != 0)
        return -EOVERFLOW;
    if (bytes > seen->limit - seen->bytes_used) return 1;
    seen_node_t **buckets = leaderboard_alloc_calloc(next_count, sizeof(*buckets));
    if (buckets == NULL) return -ENOMEM;
    for (size_t i = 0; i < seen->bucket_count; ++i) {
        seen_node_t *p = seen->buckets[i];
        while (p != NULL) {
            seen_node_t *next = p->next;
            size_t slot = (size_t)(p->hash % (uint64_t)next_count);
            p->next = buckets[slot];
            buckets[slot] = p;
            p = next;
        }
    }
    const size_t old_bytes = seen->bucket_count * sizeof(*seen->buckets);
    free(seen->buckets);
    seen->buckets = buckets;
    seen->bucket_count = next_count;
    seen->bytes_used += bytes - old_bytes;
    return 0;
}

/* Never compare raw fixture_t object representations: C permits padding. */
static bool canonical_equal(const fixture_t *lhs, const fixture_t *rhs,
                            size_t bytes) {
    const size_t count = bytes / sizeof(fixture_t);
    for (size_t i = 0U; i < count; ++i)
        if (lhs[i].home != rhs[i].home || lhs[i].away != rhs[i].away)
            return false;
    return true;
}

static bool seen_contains(const seen_table_t *seen, uint64_t hash,
                          const fixture_t *canonical) {
    if (seen->bucket_count == 0U) return false;
    size_t slot = (size_t)(hash % (uint64_t)seen->bucket_count);
    for (seen_node_t *p = seen->buckets[slot]; p != NULL; p = p->next)
        if (p->hash == hash &&
            canonical_equal(p->fixtures, canonical, seen->fixture_bytes))
            return true;
    return false;
}

static int seen_add(seen_table_t *seen, uint64_t hash,
                    const fixture_t *canonical) {
    if (seen->bucket_count == 0U) {
        int status = seen_resize(seen, 16U);
        if (status != 0) return status;
    } else if (seen->count >= seen->bucket_count - seen->bucket_count / 4U) {
        if (seen->bucket_count > SIZE_MAX / 2U) return -EOVERFLOW;
        int status = seen_resize(seen, seen->bucket_count * 2U);
        if (status != 0) return status;
    }
    if (seen->fixture_bytes > SIZE_MAX - sizeof(seen_node_t)) return -EOVERFLOW;
    size_t bytes = sizeof(seen_node_t) + seen->fixture_bytes;
    if (bytes > seen->limit - seen->bytes_used) return 1;
    seen_node_t *node = leaderboard_alloc_malloc(bytes);
    if (node == NULL) return -ENOMEM;
    node->hash = hash;
    memcpy(node->fixtures, canonical, seen->fixture_bytes);
    size_t slot = (size_t)(hash % (uint64_t)seen->bucket_count);
    node->next = seen->buckets[slot];
    seen->buckets[slot] = node;
    seen->bytes_used += bytes;
    seen->count++;
    return 0;
}

static bool fixture_after(fixture_t a, fixture_t b) {
    return a.home > b.home || (a.home == b.home && a.away > b.away);
}

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    for (unsigned j = 0U; j < 4U; ++j) {
        hash ^= (uint8_t)(value >> (8U * j));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

/* Canonical directed fixture order, ignoring fixture-storage ordering only.
 * The prefix is deliberate: future rounds cannot affect provisional results. */
static int canonicalize(const calendar_t *calendar, size_t rounds,
                        size_t teams, fixture_t *out, uint64_t *out_hash) {
    const size_t per_round = teams / 2U;
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = hash_u32(hash, (uint32_t)teams);
    hash = hash_u32(hash, (uint32_t)rounds);
    for (size_t ri = 0; ri < rounds; ++ri) {
        fixture_t *dst = out + ri * per_round;
        for (size_t fi = 0; fi < per_round; ++fi) {
            int status = calendar_fixture_at(calendar, ri, fi, &dst[fi]);
            if (status != 0) return status;
        }
        for (size_t fi = 1; fi < per_round; ++fi) {
            fixture_t item = dst[fi];
            size_t k = fi;
            while (k > 0U && fixture_after(dst[k - 1U], item)) {
                dst[k] = dst[k - 1U];
                --k;
            }
            dst[k] = item;
        }
        for (size_t fi = 0; fi < per_round; ++fi) {
            hash = hash_u32(hash, dst[fi].home);
            hash = hash_u32(hash, dst[fi].away);
        }
    }
    *out_hash = hash;
    return 0;
}

monte_carlo_options_t monte_carlo_options_default(void) {
    monte_carlo_options_t options = {0};
    options.calendar = calendar_options_default();
    options.scope = SEASON_RESOLVE_FINALIZED_PREFIX;
    options.ranking_backend = SEASON_RANKING_HEAPSORT;
    options.requested_samples = 1000U;
    options.max_generation_attempts = 1000U;
    options.progress_every_attempts = 1000U;
    return options;
}

void monte_carlo_result_destroy(monte_carlo_result_t *result) {
    if (result == NULL) return;
    season_resolution_destroy(result->baseline);
    free(result->stats);
    free(result->points_m2);
    free(result->positions);
    free(result);
}

static int create_result(const monte_carlo_options_t *options,
                         season_resolution_t *baseline,
                         monte_carlo_result_t **out) {
    size_t teams = season_resolution_team_count(baseline);
    size_t positions_count;
    if (checked_multiply(teams, teams, &positions_count) != 0 ||
        teams > SIZE_MAX / sizeof(monte_carlo_team_stats_t) ||
        teams > SIZE_MAX / sizeof(double) ||
        positions_count > SIZE_MAX / sizeof(size_t)) return -EOVERFLOW;
    monte_carlo_result_t *result = leaderboard_alloc_calloc(1U, sizeof(*result));
    if (result == NULL) return -ENOMEM;
    result->options = *options;
    result->teams = teams;
    result->rounds = season_resolution_round_count(baseline);
    result->stats = leaderboard_alloc_calloc(teams, sizeof(*result->stats));
    result->points_m2 = leaderboard_alloc_calloc(teams, sizeof(*result->points_m2));
    result->positions = leaderboard_alloc_calloc(positions_count, sizeof(*result->positions));
    if (result->stats == NULL || result->points_m2 == NULL || result->positions == NULL) {
        monte_carlo_result_destroy(result);
        return -ENOMEM;
    }
    for (size_t i = 0; i < teams; ++i) {
        int status = season_resolution_team_uuid_at(baseline, (team_index_t)i,
                                                     &result->stats[i].uuid);
        if (status != 0) {
            monte_carlo_result_destroy(result);
            return status;
        }
    }
    result->baseline = baseline; /* transfer ownership only on success */
    *out = result;
    return 0;
}

static int add_observation(monte_carlo_result_t *result,
                           const season_resolution_t *sample) {
    const size_t n = result->teams;
    const size_t new_count = result->accepted + 1U;
    for (size_t pos = 0; pos < n; ++pos) {
        season_rank_entry_t e;
        int status = season_resolution_rank_at(sample, pos, &e);
        if (status != 0) return status;
        if (e.team_index >= n || e.sporting_rank == 0U || e.sporting_rank > n)
            return -EINVAL;
        monte_carlo_team_stats_t *s = &result->stats[e.team_index];
        const double value = (double)e.standings.championship_points;
        const double gd = (double)e.goal_difference;
        if (s->observations == SIZE_MAX ||
            result->positions[(size_t)e.team_index * n + e.sporting_rank - 1U] == SIZE_MAX ||
            (e.sporting_rank == 1U && s->first_place_finishes == SIZE_MAX))
            return -EOVERFLOW;
        if (s->observations == 0U) {
            s->min_points = s->max_points = e.standings.championship_points;
        } else {
            if (e.standings.championship_points < s->min_points)
                s->min_points = e.standings.championship_points;
            if (e.standings.championship_points > s->max_points)
                s->max_points = e.standings.championship_points;
        }
        const double delta = value - s->mean_points;
        s->mean_points += delta / (double)new_count;
        result->points_m2[e.team_index] += delta * (value - s->mean_points);
        s->mean_goal_difference += (gd - s->mean_goal_difference) / (double)new_count;
        s->observations = new_count;
        s->has_observations = true;
        result->positions[(size_t)e.team_index * n + e.sporting_rank - 1U]++;
        if (e.sporting_rank == 1U) s->first_place_finishes++;
    }
    result->accepted = new_count;
    return 0;
}

static int write_observation_csv(FILE *stream, const monte_carlo_result_t *result,
                                 const season_resolution_t *sample,
                                 uint64_t hash, size_t attempt) {
    for (size_t pos = 0; pos < result->teams; ++pos) {
        season_rank_entry_t e;
        int status = season_resolution_rank_at(sample, pos, &e);
        if (status != 0) return status;
        char uuid[TEAM_UUID_TEXT_SIZE];
        if (team_uuid_format(&result->stats[e.team_index].uuid, uuid) != 0)
            return -EINVAL;
        if (fprintf(stream, "%zu,%zu,%016" PRIx64 ",%s,%" PRIu32 ",%" PRIu32
                    ",%" PRIu32 ",%" PRId64 ",%" PRIu32 ",%" PRIu32 "\n",
                    result->accepted, attempt, hash, uuid, e.sporting_rank,
                    e.standings.championship_points, e.fantasy_total_tenths,
                    e.goal_difference, e.goal_stats.goals_for,
                    e.goal_stats.goals_against) < 0) return -EIO;
    }
    return 0;
}

int monte_carlo_run(const competition_t *competition,
                    const match_rules_t *rules,
                    const monte_carlo_options_t *given,
                    FILE *observation_csv,
                    monte_carlo_progress_fn progress,
                    void *progress_context,
                    monte_carlo_result_t **out) {
    if (out == NULL) return -EINVAL;
    *out = NULL;
    const monte_carlo_options_t defaults = monte_carlo_options_default();
    const monte_carlo_options_t *options = given != NULL ? given : &defaults;
    if (competition == NULL || match_rules_validate(rules) != 0 ||
        (options->scope != SEASON_RESOLVE_FULL_SEASON &&
         options->scope != SEASON_RESOLVE_FINALIZED_PREFIX) ||
        (options->ranking_backend != SEASON_RANKING_HEAPSORT &&
         options->ranking_backend != SEASON_RANKING_AVL) ||
        options->requested_samples == 0U || options->max_generation_attempts == 0U ||
        (options->unique_only && !options->track_uniqueness) ||
        (options->track_uniqueness && options->uniqueness_memory_limit_bytes == 0U))
        return -EINVAL;

    const season_resolution_options_t resolution_options = {
        options->scope, options->ranking_backend
    };
    season_resolution_t *baseline = NULL;
    int status = season_resolve_with_options(competition, NULL, rules,
                                             &resolution_options, &baseline);
    if (status != 0) return status;
    if (season_resolution_round_count(baseline) == 0U) {
        season_resolution_destroy(baseline);
        return -EAGAIN;
    }
    monte_carlo_result_t *result = NULL;
    status = create_result(options, baseline, &result);
    if (status != 0) { season_resolution_destroy(baseline); return status; }

    calendar_generator_t *generator = NULL;
    status = calendar_generator_create(competition, &options->calendar, &generator);
    if (status != 0) goto failure;

    const size_t fixtures_per_round = result->teams / 2U;
    size_t fixture_count, fixture_bytes;
    status = checked_multiply(result->rounds, fixtures_per_round, &fixture_count);
    if (status != 0) goto failure;
    status = checked_multiply(fixture_count, sizeof(fixture_t), &fixture_bytes);
    if (status != 0) goto failure;
    fixture_t *canonical = leaderboard_alloc_malloc(fixture_bytes);
    if (canonical == NULL) { status = -ENOMEM; goto failure; }
    seen_table_t seen = {0};
    seen.limit = options->uniqueness_memory_limit_bytes;
    seen.fixture_bytes = fixture_bytes;

    if (observation_csv != NULL &&
        fprintf(observation_csv,"sample,attempt,evaluated_directed_fingerprint,team_uuid,"
                "sporting_rank,championship_points,fantasy_points_tenths,"
                "goal_difference,goals_for,goals_against\n") < 0) {
        status = -EIO;
        goto loop_failure;
    }
    while (result->attempted < options->max_generation_attempts &&
           result->accepted < options->requested_samples) {
        calendar_t *calendar = NULL;
        season_resolution_t *resolution = NULL;
        calendar_diagnostics_t draw_diagnostics = {0};
        status = calendar_generator_next(generator, &calendar, &draw_diagnostics);
        if (status != 0) goto loop_failure;
        result->attempted++;
        status = accumulate_generation(&result->generation, &draw_diagnostics);
        if (status != 0) { calendar_destroy(calendar); goto loop_failure; }
        uint64_t hash = 0U;
        status = canonicalize(calendar, result->rounds, result->teams, canonical, &hash);
        if (status != 0) { calendar_destroy(calendar); goto loop_failure; }
        bool duplicate = false;
        if (options->track_uniqueness) {
            duplicate = seen_contains(&seen, hash, canonical);
            if (duplicate) {
                result->duplicates++;
            } else {
                const int put = seen_add(&seen, hash, canonical);
                if (put == 1) {
                    result->completion = MONTE_CARLO_UNIQUENESS_MEMORY_LIMIT;
                    calendar_destroy(calendar);
                    break;
                }
                if (put != 0) {
                    calendar_destroy(calendar);
                    status = put;
                    goto loop_failure;
                }
                result->unique++;
            }
        }
        if (!duplicate || !options->unique_only) {
            status = season_resolve_with_options(competition, calendar, rules,
                                                 &resolution_options, &resolution);
            calendar_destroy(calendar);
            calendar = NULL;
            if (status != 0) goto loop_failure;
            status = add_observation(result, resolution);
            if (status == 0 && observation_csv != NULL)
                status = write_observation_csv(observation_csv, result, resolution,
                                               hash, result->attempted);
            season_resolution_destroy(resolution);
            if (status != 0) goto loop_failure;
        } else {
            calendar_destroy(calendar);
        }
        if (progress != NULL && options->progress_every_attempts != 0U &&
            result->attempted % options->progress_every_attempts == 0U &&
            result->accepted < options->requested_samples) {
            const monte_carlo_progress_t info = {
                result->attempted, result->accepted, result->duplicates
            };
            if (!progress(&info, progress_context)) {
                result->completion = MONTE_CARLO_CANCELLED;
                break;
            }
        }
    }
    if (result->completion == MONTE_CARLO_COMPLETED &&
        result->accepted < options->requested_samples)
        result->completion = MONTE_CARLO_ATTEMPT_LIMIT;
    if (progress != NULL && options->progress_every_attempts == 0U) {
        const monte_carlo_progress_t info = {
            result->attempted, result->accepted, result->duplicates
        };
        (void)progress(&info, progress_context); /* terminal notification only */
    }
    for (size_t i = 0; i < result->teams; ++i) {
        monte_carlo_team_stats_t *s = &result->stats[i];
        if (s->observations != 0U)
            s->population_variance_points = result->points_m2[i] /
                                             (double)s->observations;
    }
    seen_destroy(&seen);
    free(canonical);
    calendar_generator_destroy(generator);
    *out = result;
    return 0;
loop_failure:
    seen_destroy(&seen);
    free(canonical);
failure:
    calendar_generator_destroy(generator);
    monte_carlo_result_destroy(result);
    return status;
}

monte_carlo_completion_t monte_carlo_result_completion(const monte_carlo_result_t *r) {
    return r == NULL ? MONTE_CARLO_ATTEMPT_LIMIT : r->completion;
}
size_t monte_carlo_result_requested(const monte_carlo_result_t *r) {
    return r == NULL ? 0U : r->options.requested_samples;
}
size_t monte_carlo_result_attempts(const monte_carlo_result_t *r) {
    return r == NULL ? 0U : r->attempted;
}
size_t monte_carlo_result_accepted(const monte_carlo_result_t *r) {
    return r == NULL ? 0U : r->accepted;
}
size_t monte_carlo_result_evaluated_rounds(const monte_carlo_result_t *r) {
    return r == NULL ? 0U : r->rounds;
}
size_t monte_carlo_result_team_count(const monte_carlo_result_t *r) {
    return r == NULL ? 0U : r->teams;
}
bool monte_carlo_result_uniqueness_tracked(const monte_carlo_result_t *r) {
    return r != NULL && r->options.track_uniqueness;
}
int monte_carlo_result_uniqueness(const monte_carlo_result_t *r,
                                  size_t *unique, size_t *duplicates) {
    if (r == NULL || unique == NULL || duplicates == NULL) return -EINVAL;
    if (!r->options.track_uniqueness) return -ENOTSUP;
    *unique = r->unique;
    *duplicates = r->duplicates;
    return 0;
}
int monte_carlo_result_calendar_diagnostics(const monte_carlo_result_t *r,
                                            calendar_diagnostics_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    *out = r->generation;
    return 0;
}

int monte_carlo_result_team_at(const monte_carlo_result_t *r,
                               team_index_t i, monte_carlo_team_stats_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    if (i >= r->teams) return -ERANGE;
    *out = r->stats[i];
    return 0;
}
int monte_carlo_result_rank_frequency(const monte_carlo_result_t *r,
                                      team_index_t i, uint32_t rank, size_t *out) {
    if (r == NULL || out == NULL) return -EINVAL;
    if (i >= r->teams || rank == 0U || rank > r->teams) return -ERANGE;
    *out = r->positions[(size_t)i * r->teams + rank - 1U];
    return 0;
}
const season_resolution_t *monte_carlo_result_baseline(const monte_carlo_result_t *r) {
    return r == NULL ? NULL : r->baseline;
}

int monte_carlo_write_summary_csv(const monte_carlo_result_t *r, FILE *stream) {
    if (r == NULL || stream == NULL) return -EINVAL;
    if (fprintf(stream,"team_uuid,completion,requested,attempts,accepted,"
               "evaluated_rounds,seed,method,sampling_mode,uniqueness_tracked,"
               "unique_calendars,duplicate_attempts,two_round_attempts,"
               "two_round_partial_moves,three_round_moves,"
               "three_round_search_exhaustions,baseline_rank,baseline_points,"
               "first_place_finishes,min_points,max_points,mean_points,"
               "population_variance_points,mean_goal_difference") < 0) return -EIO;
    for (size_t rank = 1U; rank <= r->teams; ++rank)
        if (fprintf(stream,",rank_%zu_count",rank) < 0) return -EIO;
    if (fputc('\n',stream) == EOF) return -EIO;

    for (size_t i = 0U; i < r->teams; ++i) {
        char uuid[TEAM_UUID_TEXT_SIZE];
        if (team_uuid_format(&r->stats[i].uuid, uuid) != 0) return -EINVAL;
        season_rank_entry_t baseline_rank;
        size_t pos = 0U;
        for (; pos < r->teams; ++pos) {
            if (season_resolution_rank_at(r->baseline,pos,&baseline_rank) != 0)
                return -EINVAL;
            if (baseline_rank.team_index == i) break;
        }
        if (pos == r->teams) return -EINVAL;
        if (fprintf(stream,"%s,%d,%zu,%zu,%zu,%zu,%" PRIu32 ",%d,%d,%d,",
                    uuid,(int)r->completion,r->options.requested_samples,
                    r->attempted,r->accepted,r->rounds,r->options.calendar.seed,
                    (int)r->options.calendar.method,
                    (int)r->options.calendar.sampling_mode,
                    (int)r->options.track_uniqueness) < 0) return -EIO;
        if (r->options.track_uniqueness) {
            if (fprintf(stream,"%zu,%zu,",r->unique,r->duplicates) < 0) return -EIO;
        } else if (fputs(",,",stream) == EOF) return -EIO;
        if (fprintf(stream,"%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",",
                    r->generation.two_round_attempts,
                    r->generation.two_round_partial_moves,
                    r->generation.three_round_moves,
                    r->generation.three_round_search_exhaustions) < 0) return -EIO;
        if (fprintf(stream,"%" PRIu32 ",%" PRIu32 ",%zu,",baseline_rank.sporting_rank,
                    baseline_rank.standings.championship_points,
                    r->stats[i].first_place_finishes) < 0) return -EIO;
        if (r->stats[i].has_observations) {
            if (fprintf(stream,"%" PRIu32 ",%" PRIu32 ",%.17g,%.17g,%.17g",
                        r->stats[i].min_points,r->stats[i].max_points,
                        r->stats[i].mean_points,
                        r->stats[i].population_variance_points,
                        r->stats[i].mean_goal_difference) < 0) return -EIO;
        } else if (fputs(",,,,",stream) == EOF) return -EIO;
        for (size_t rank = 0U; rank < r->teams; ++rank)
            if (fprintf(stream,",%zu",r->positions[i*r->teams+rank]) < 0)
                return -EIO;
        if (fputc('\n',stream) == EOF) return -EIO;
    }
    return ferror(stream) ? -EIO : 0;
}
