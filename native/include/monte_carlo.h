/*
 * LeaderboardSystem reproducible calendar experiments (milestone 3.1.1).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_MONTE_CARLO_H
#define LEADERBOARD_MONTE_CARLO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "calendar.h"
#include "season_resolution.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum monte_carlo_completion {
    MONTE_CARLO_COMPLETED = 0,
    MONTE_CARLO_ATTEMPT_LIMIT = 1,
    MONTE_CARLO_CANCELLED = 2,
    MONTE_CARLO_UNIQUENESS_MEMORY_LIMIT = 3
} monte_carlo_completion_t;

typedef struct monte_carlo_progress {
    size_t generation_attempts;
    size_t accepted_samples;
    size_t duplicate_attempts; /* meaningful only when tracking uniqueness */
} monte_carlo_progress_t;

/* Return false to request cooperative cancellation after the current attempt.
 * Callback executes synchronously. Do not reenter this batch from callback. */
typedef bool (*monte_carlo_progress_fn)(const monte_carlo_progress_t *progress,
                                        void *context);

typedef struct monte_carlo_options {
    calendar_options_t calendar;
    season_resolution_scope_t scope;
    size_t requested_samples;         /* >0 */
    size_t max_generation_attempts;   /* finite, >0; includes rejected draws */
    bool unique_only;                 /* exclude identical evaluated schedules */
    bool track_uniqueness;            /* exact diagnostics; required if unique_only */
    size_t uniqueness_memory_limit_bytes; /* >0 if tracking enabled */
    size_t progress_every_attempts;   /* 0: callback only at end; otherwise interval */
    season_ranking_backend_t ranking_backend; /* default HEAPSORT; baseline + draws */
} monte_carlo_options_t;

typedef struct monte_carlo_team_stats {
    team_uuid_t uuid;
    size_t observations;
    size_t first_place_finishes;  /* shared sporting ranks count for each team */
    bool has_observations;
    uint32_t min_points;
    uint32_t max_points;
    double mean_points;
    double population_variance_points; /* divide by observations, NOT n-1 */
    double mean_goal_difference;
} monte_carlo_team_stats_t;

typedef struct monte_carlo_result monte_carlo_result_t;

/* Defaults: 1000 duplicate-inclusive independent draws, 1000 attempts,
 * no exact uniqueness diagnostics, finalized-prefix scope. */
monte_carlo_options_t monte_carlo_options_default(void);

/* One seeded generator, one synchronous batch; source and rules are borrowed.
 * The result owns an independent original-calendar baseline and team summaries.
 * A finalized prefix of length zero is rejected with -EAGAIN. The original
 * baseline never consumes a draw; a generator's copy of it does count.
 *
 * observation_csv (optional) receives a header and one row per team per
 * accepted sample. The stream remains caller-owned. Errors in I/O return
 * -EIO, clear *out, and may leave a partial CSV. Caller must disregard it.
 *
 * Completion statuses including cancellation and budgets return 0 and a
 * partial (possibly zero-sample) result. Real errors return negative errno,
 * clear *out and free all private allocations. */
int monte_carlo_run(const competition_t *competition,
                    const match_rules_t *rules,
                    const monte_carlo_options_t *options,
                    FILE *observation_csv,
                    monte_carlo_progress_fn progress,
                    void *progress_context,
                    monte_carlo_result_t **out);

void monte_carlo_result_destroy(monte_carlo_result_t *result);
monte_carlo_completion_t monte_carlo_result_completion(const monte_carlo_result_t *result);
size_t monte_carlo_result_requested(const monte_carlo_result_t *result);
size_t monte_carlo_result_attempts(const monte_carlo_result_t *result);
size_t monte_carlo_result_accepted(const monte_carlo_result_t *result);
size_t monte_carlo_result_evaluated_rounds(const monte_carlo_result_t *result);
size_t monte_carlo_result_team_count(const monte_carlo_result_t *result);
bool monte_carlo_result_uniqueness_tracked(const monte_carlo_result_t *result);
/* Return -ENOTSUP if tracking was disabled; otherwise exact counts. */
int monte_carlo_result_uniqueness(const monte_carlo_result_t *result,
                                  size_t *out_unique, size_t *out_duplicates);
/* Sum of generator diagnostics over ALL successful generation attempts,
 * including repeated/rejected calendars. */
int monte_carlo_result_calendar_diagnostics(const monte_carlo_result_t *result,
                                            calendar_diagnostics_t *out);
int monte_carlo_result_team_at(const monte_carlo_result_t *result,
                               team_index_t team_index,
                               monte_carlo_team_stats_t *out);
/* rank is 1-based competition sporting rank. Missing ranks have frequency 0. */
int monte_carlo_result_rank_frequency(const monte_carlo_result_t *result,
                                      team_index_t team_index,
                                      uint32_t rank, size_t *out_count);
/* Borrowed read-only baseline, valid for result's lifetime. */
const season_resolution_t *monte_carlo_result_baseline(const monte_carlo_result_t *result);
/* Summary CSV includes metadata, baseline position, per-team descriptive
 * statistics and columns for all possible sporting ranks. Caller owns stream. */
int monte_carlo_write_summary_csv(const monte_carlo_result_t *result, FILE *stream);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_MONTE_CARLO_H */
