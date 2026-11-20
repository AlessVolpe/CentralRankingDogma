/*
 * LeaderboardSystem private JSON Monte Carlo adapter (3.1.3 PR C).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_SIMULATION_CLI_H
#define LEADERBOARD_SIMULATION_CLI_H

#include "json_value.h"
#include "monte_carlo.h"

typedef struct simulation_error {
    int exit_code;
    const char *code;
    const char *message;
    const char *path;
} simulation_error_t;

typedef struct simulation_csv {
    char *staging_path;
    char *destination_path;
    bool created;
} simulation_csv_t;

/* No file is published on a failed simulation or failed JSON serialization.
 * A successful simulation with accepted_samples < requested_samples is valid.
 * The caller owns the returned JSON and must call simulation_csv_discard()
 * whether publication succeeds or not. */
int simulation_cli_run(const json_value_t *request,
                       const competition_t *competition,
                       const match_rules_t *rules,
                       const monte_carlo_options_t *options,
                       const char *request_id,
                       const char *observations_destination,
                       json_value_t **out_result,
                       simulation_csv_t *out_csv,
                       simulation_error_t *out_error);

/* Publish only after final response serialization has succeeded. Destination
 * paths are trusted subprocess arguments, never JSON fields. */
int simulation_csv_commit(simulation_csv_t *csv);
void simulation_csv_discard(simulation_csv_t *csv);

#endif /* LEADERBOARD_SIMULATION_CLI_H */
