/*
 * LeaderboardSystem 3.1.2 domain experiment runner (public C11 APIs).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Provenance: synthetic FIXED performances or locally supplied pseudonymized
 * competition TSV. No names/source IDs are emitted in experimental CSV.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "competition.h"
#include "calendar.h"
#include "match.h"
#include "monte_carlo.h"
#include "season_resolution.h"
#ifdef CRD_RESEARCH_PROFILE
#include "research_profile_internal.h"
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define MAX_N 12U
#define MAX_R 40U
#define MAX_F (MAX_N / 2U)

typedef struct fixture_data {
    char ids[MAX_N][TEAM_UUID_TEXT_SIZE];
    char names[MAX_N][32];
    competition_team_input_t teams[MAX_N];
    competition_fixture_input_t fixtures[MAX_R][MAX_F];
    competition_round_input_t rounds[MAX_R];
    competition_score_input_t scores[MAX_R * MAX_N];
    competition_input_t input;
} fixture_data_t;

static void check(int ok, const char *why) {
    if (!ok) { fprintf(stderr, "season_experiment: %s\n", why); exit(EXIT_FAILURE); }
}
static double wall_time(void) {
#ifdef _WIN32
    LARGE_INTEGER a, b;
    check(QueryPerformanceCounter(&a) && QueryPerformanceFrequency(&b) && b.QuadPart > 0,
          "high-resolution clock unavailable");
    return (double)a.QuadPart / (double)b.QuadPart;
#else
    struct timespec t;
    check(clock_gettime(CLOCK_MONOTONIC, &t) == 0, "monotonic clock unavailable");
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
#endif
}
static double cpu_time(void) {
    clock_t t = clock();
    check(t != (clock_t)-1, "CPU clock unavailable");
    return (double)t / CLOCKS_PER_SEC;
}
static uint32_t random32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}
static size_t number(const char *text, size_t maximum) {
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(text, &end, 10);
    check(errno == 0 && end != text && *end == '\0' && text[0] != '-' &&
          v > 0 && v <= maximum, "invalid numeric option");
    return (size_t)v;
}
static void create_synthetic(fixture_data_t *f, size_t n, uint32_t seed) {
    size_t positions[MAX_N];
    memset(f, 0, sizeof(*f));
    check(n >= 2U && n <= MAX_N && n % 2U == 0U, "synthetic size must be even 2..12");
    for (size_t i = 0; i < n; ++i) {
        check(snprintf(f->ids[i], sizeof(f->ids[i]),
                       "00000000-0000-4000-8000-%012u", (unsigned)(i + 1U)) > 0,
              "UUID formatting");
        check(snprintf(f->names[i], sizeof(f->names[i]), "Team-%03u", (unsigned)i) > 0,
              "team name formatting");
        f->teams[i] = (competition_team_input_t){f->ids[i], f->names[i]};
        positions[i] = i;
    }
    size_t periods = n - 1U;
    for (size_t round = 0; round < periods; ++round) {
        size_t day = round + 1U;
        for (size_t i = 0; i < n / 2U; ++i) {
            size_t a = positions[i], b = positions[n - i - 1U];
            /* Deterministic home/away pattern, not a historical league. */
            if ((round + i) % 2U != 0U) { size_t tmp = a; a = b; b = tmp; }
            f->fixtures[round][i] = (competition_fixture_input_t){f->ids[a], f->ids[b]};
        }
        f->rounds[round] = (competition_round_input_t){(uint32_t)day,
                                                       f->fixtures[round], n / 2U};
        for (size_t team = 0; team < n; ++team) {
            uint32_t score = 600U + random32(&seed) % 251U;
            f->scores[round * n + team] = (competition_score_input_t){
                f->ids[team], (uint32_t)day, {score, FANTASY_SCORE_FINAL}};
        }
        size_t last = positions[n - 1U];
        for (size_t i = n - 1U; i > 1U; --i) positions[i] = positions[i - 1U];
        positions[1] = last;
    }
    f->input = (competition_input_t){f->teams, n, f->rounds, periods,
                                     f->scores, n * periods};
}
static size_t find_id(const fixture_data_t *f, size_t team_count, const char *uuid) {
    for (size_t i = 0; i < team_count; ++i)
        if (strcmp(f->ids[i], uuid) == 0) return i;
    return team_count;
}
static void load_private_tsv(fixture_data_t *f, const char *filename) {
    memset(f, 0, sizeof(*f));
    FILE *file = fopen(filename, "rb");
    check(file != NULL, "cannot open local pseudonymized TSV");
    char line[256], uuid1[TEAM_UUID_TEXT_SIZE], uuid2[TEAM_UUID_TEXT_SIZE],
         name[32], status_text[16], trailing;
    size_t n = 0U, rounds = 0U, scores = 0U;
    unsigned int round, day, points;
    while (fgets(line, sizeof(line), file) != NULL) {
        check(strchr(line, '\n') != NULL || feof(file), "overlong TSV line");
        line[strcspn(line, "\r\n")] = '\0';
        if (sscanf(line, "TEAM\t%36s\t%31s %c", uuid1, name, &trailing) == 2) {
            check(n < MAX_N && find_id(f, n, uuid1) == n, "duplicate/too many teams");
            memcpy(f->ids[n], uuid1, strlen(uuid1) + 1U);
            memcpy(f->names[n], name, strlen(name) + 1U);
            f->teams[n] = (competition_team_input_t){f->ids[n], f->names[n]};
            ++n;
        } else if (sscanf(line, "FIXTURE\t%u\t%u\t%36s\t%36s %c",
                          &round, &day, uuid1, uuid2, &trailing) == 4) {
            check(round < MAX_R && round <= rounds && day > 0U, "invalid fixture round");
            if (f->rounds[round].fixture_count == 0U) {
                f->rounds[round].serie_a_matchday = day;
                f->rounds[round].fixtures = f->fixtures[round];
                ++rounds;
            }
            check(f->rounds[round].fixture_count < MAX_F &&
                  f->rounds[round].serie_a_matchday == day &&
                  find_id(f, n, uuid1) < n && find_id(f, n, uuid2) < n,
                  "invalid fixture participants/day");
            f->fixtures[round][f->rounds[round].fixture_count++] =
                (competition_fixture_input_t){f->ids[find_id(f,n,uuid1)],
                                              f->ids[find_id(f,n,uuid2)]};
        } else if (sscanf(line, "SCORE\t%u\t%36s\t%u\t%15s %c",
                          &round, uuid1, &points, status_text, &trailing) == 4) {
            check(round < rounds && scores < MAX_N * MAX_R &&
                  find_id(f,n,uuid1) < n, "invalid score row");
            fantasy_score_status_t state;
            if (strcmp(status_text,"FINAL") == 0) state = FANTASY_SCORE_FINAL;
            else { check(strcmp(status_text,"PENDING") == 0 && points == 0U,
                         "invalid score state"); state = FANTASY_SCORE_PENDING; }
            f->scores[scores++] = (competition_score_input_t){
                f->ids[find_id(f,n,uuid1)], f->rounds[round].serie_a_matchday,
                {points, state}};
        } else check(0, "unrecognized private TSV row");
    }
    check(fclose(file) == 0, "closing TSV failed");
    check(n >= 2U && n % 2U == 0U && rounds >= 1U &&
          scores == n * rounds, "incomplete/invalid competition TSV");
    f->input = (competition_input_t){f->teams, n, f->rounds, rounds, f->scores, scores};
}
static void write_stage(FILE *out, const char *dataset, const char *backend,
                        const char *phase, size_t n, size_t operations, uint32_t seed,
                        double wall, double cpu, uint64_t check_value) {
    check(fprintf(out, "domain,%s,%s,%s,%zu,%zu,0,%" PRIu32 ",%.9f,%.9f,%" PRIu64 ",,\n",
                  dataset, backend, phase, n, operations, seed, wall, cpu, check_value) > 0,
          "writing stage CSV");
}
int main(int argc, char **argv) {
    if (argc != 9) {
        fprintf(stderr,"usage: %s <synthetic-size:8|10|12 OR competition.tsv> "
                "<seed> <samples> <heap|avl> <shuffle|recombine> "
                "<independent|chain> <output-observations.csv> <output-summary.csv>\n",argv[0]);
        return EXIT_FAILURE;
    }
    fixture_data_t *f = calloc(1U, sizeof(*f));
    check(f != NULL, "fixture allocation");
    uint32_t seed = (uint32_t)number(argv[2], UINT32_MAX);
    size_t samples = number(argv[3], 1000000U);
    const season_ranking_backend_t backend = strcmp(argv[4], "heap") == 0 ?
        SEASON_RANKING_HEAPSORT : SEASON_RANKING_AVL;
    check(strcmp(argv[4], "heap") == 0 || strcmp(argv[4], "avl") == 0,
          "unknown ranking backend");
    const char *backend_name = backend == SEASON_RANKING_AVL ? "avl" : "heapsort";
    calendar_method_t method = strcmp(argv[5], "shuffle") == 0 ?
        CALENDAR_METHOD_ROUND_SHUFFLE : CALENDAR_METHOD_RECOMBINE;
    check(strcmp(argv[5], "shuffle") == 0 || strcmp(argv[5], "recombine") == 0,
          "unknown calendar method");
    calendar_sampling_mode_t sampling = strcmp(argv[6], "independent") == 0 ?
        CALENDAR_SAMPLING_INDEPENDENT : CALENDAR_SAMPLING_CHAIN;
    check(strcmp(argv[6], "independent") == 0 || strcmp(argv[6], "chain") == 0,
          "unknown sampling mode");
    const char *dataset_label = "synthetic";
    if (strcmp(argv[1], "8") == 0 || strcmp(argv[1], "10") == 0 ||
        strcmp(argv[1], "12") == 0) {
        const size_t n = number(argv[1], MAX_N);
        /* Fixed dataset seed! Replicate seeds select calendars only, never
         * regenerate the historical-performance matrix being compared. */
        create_synthetic(f, n, UINT32_C(0xC2B2AE35) ^ (uint32_t)n);
    } else { load_private_tsv(f, argv[1]); dataset_label = "private"; }
    competition_t *competition = NULL;
    check(competition_create(&f->input, &competition) == 0, "competition invalid");
    match_rules_t rules;
    check(match_rules_default(&rules) == 0, "default rules unavailable");
    monte_carlo_options_t options = monte_carlo_options_default();
    options.ranking_backend = backend;
    options.scope = SEASON_RESOLVE_FINALIZED_PREFIX;
    options.calendar.seed = seed;
    options.calendar.method = method;
    options.calendar.sampling_mode = sampling;
    options.requested_samples = samples;
    options.max_generation_attempts = samples;
    options.track_uniqueness = true;
    options.uniqueness_memory_limit_bytes = SIZE_MAX / 16U;
    FILE *observations = fopen(argv[7], "wb");
    check(observations != NULL, "cannot open observation output");
    monte_carlo_result_t *result = NULL;
#ifdef CRD_RESEARCH_PROFILE
    leaderboard_profile_reset();
#endif
    double start_wall = wall_time(), start_cpu = cpu_time();
    int status = monte_carlo_run(competition, &rules, &options, observations,
                                 NULL, NULL, &result);
    double whole_wall = wall_time() - start_wall, whole_cpu = cpu_time() - start_cpu;
    check(fclose(observations) == 0, "observation output close failed");
    check(status == 0 && result != NULL, "monte_carlo_run failed");
    check(monte_carlo_result_completion(result) == MONTE_CARLO_COMPLETED &&
          monte_carlo_result_accepted(result) == samples,
          "Monte Carlo incomplete; do not analyze truncated experiments");
    FILE *summary = fopen(argv[8], "wb");
    check(summary != NULL, "cannot open summary output");
    check(monte_carlo_write_summary_csv(result, summary) == 0 && fclose(summary) == 0,
          "summary CSV write failed");
    printf("experiment,dataset,backend,phase,count,operations,replication,seed,"
           "wall_seconds,cpu_seconds,checksum,wrapper_allocations,wrapper_requested_bytes\n");
#ifdef CRD_RESEARCH_PROFILE
    static const char *names[] = {
        "profile_generation", "profile_resolution_including_ranking",
        "profile_ranking", "profile_aggregation", "profile_canonicalization"
    };
    for (int k = 0; k < LEADERBOARD_PROFILE_STAGE_COUNT; ++k) {
        leaderboard_profile_sample_t m;
        check(leaderboard_profile_read((leaderboard_profile_stage_t)k, &m) == 0,
              "research profiler snapshot failed");
        check(fprintf(stdout,
                      "profile,%s,%s,%s,%zu,%" PRIu64 ",0,%" PRIu32
                      ",%.9f,%.9f,0,%" PRIu64 ",%" PRIu64 "\n",
                      dataset_label, backend_name, names[k], f->input.team_count,
                      m.calls, seed, m.wall_seconds, m.cpu_seconds,
                      m.wrapper_allocations, m.wrapper_requested_bytes) > 0,
              "profile CSV write failed");
    }
#endif
    write_stage(stdout, dataset_label, backend_name, "monte_full",
                f->input.team_count, samples, seed, whole_wall, whole_cpu, samples);
    /* A separate controlled trial measures the public API stage boundaries.
     * It is not claimed to be an internal decomposition of monte_carlo_run. */
    calendar_options_t calendar_options = options.calendar;
    calendar_generator_t *generator = NULL;
    check(calendar_generator_create(competition, &calendar_options, &generator) == 0,
          "calendar generator failed");
    season_resolution_options_t season_options = {options.scope, backend};
    double gen_wall = 0, gen_cpu = 0, resolve_wall = 0, resolve_cpu = 0;
    double query_wall = 0, query_cpu = 0;
    uint64_t checksum = 0U;
    for (size_t i = 0; i < samples; ++i) {
        calendar_t *calendar = NULL;
        double w = wall_time(), c = cpu_time();
        check(calendar_generator_next(generator, &calendar, NULL) == 0,
              "calendar generation failed");
        gen_wall += wall_time() - w; gen_cpu += cpu_time() - c;
        season_resolution_t *resolved = NULL;
        w = wall_time(); c = cpu_time();
        check(season_resolve_with_options(competition, calendar, &rules,
                                          &season_options, &resolved) == 0,
              "season resolution failed");
        resolve_wall += wall_time() - w; resolve_cpu += cpu_time() - c;
        w = wall_time(); c = cpu_time();
        for (size_t team = 0; team < f->input.team_count; ++team) {
            season_rank_entry_t entry;
            check(season_resolution_rank_at(resolved, team, &entry) == 0,
                  "result query failed");
            checksum += (uint64_t)entry.sporting_rank + entry.standings.championship_points;
        }
        query_wall += wall_time() - w; query_cpu += cpu_time() - c;
        season_resolution_destroy(resolved);
        calendar_destroy(calendar);
    }
    write_stage(stdout, dataset_label, backend_name, "generation",
                f->input.team_count, samples, seed, gen_wall, gen_cpu, checksum);
    write_stage(stdout, dataset_label, backend_name, "resolution_including_ranking",
                f->input.team_count, samples, seed, resolve_wall, resolve_cpu, checksum);
    write_stage(stdout, dataset_label, backend_name, "rank_access",
                f->input.team_count, samples * f->input.team_count, seed,
                query_wall, query_cpu, checksum);
    calendar_generator_destroy(generator);
    monte_carlo_result_destroy(result);
    competition_destroy(competition);
    free(f);
    return EXIT_SUCCESS;
}
