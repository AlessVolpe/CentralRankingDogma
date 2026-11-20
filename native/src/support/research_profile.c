/*
 * LeaderboardSystem opt-in internal timing and allocation diagnostics (3.1.2).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "research_profile_internal.h"

#ifdef CRD_RESEARCH_PROFILE
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_NESTING 16U

typedef struct active_stage {
    leaderboard_profile_stage_t stage;
    double wall_start;
    double cpu_start;
} active_stage_t;
static active_stage_t active[MAX_NESTING];
static size_t active_count;
static leaderboard_profile_sample_t measurements[LEADERBOARD_PROFILE_STAGE_COUNT];

static double wall_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER value, frequency;
    if (!QueryPerformanceCounter(&value) ||
        !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        abort();
    return (double)value.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) abort();
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}
static double cpu_seconds(void) {
    clock_t tick = clock();
    if (tick == (clock_t)-1) abort();
    return (double)tick / (double)CLOCKS_PER_SEC;
}
void leaderboard_profile_reset(void) {
    if (active_count != 0U) abort();
    memset(measurements, 0, sizeof(measurements));
}
void leaderboard_profile_begin(leaderboard_profile_stage_t stage) {
    if ((unsigned)stage >= LEADERBOARD_PROFILE_STAGE_COUNT ||
        active_count >= MAX_NESTING) abort();
    active_stage_t *entry = &active[active_count++];
    entry->stage = stage;
    entry->wall_start = wall_seconds();
    entry->cpu_start = cpu_seconds();
}
void leaderboard_profile_end(leaderboard_profile_stage_t stage) {
    if (!active_count || active[active_count - 1U].stage != stage) abort();
    double wall = wall_seconds(), cpu = cpu_seconds();
    const active_stage_t frame = active[--active_count];
    leaderboard_profile_sample_t *stats = &measurements[stage];
    ++stats->calls;
    stats->wall_seconds += wall - frame.wall_start;
    stats->cpu_seconds += cpu - frame.cpu_start;
}
void leaderboard_profile_record_wrapper_allocation(size_t bytes) {
    if (active_count == 0U) return;
    leaderboard_profile_sample_t *stats = &measurements[active[active_count - 1U].stage];
    ++stats->wrapper_allocations;
    if (stats->wrapper_requested_bytes > UINT64_MAX - (uint64_t)bytes)
        stats->wrapper_requested_bytes = UINT64_MAX;
    else stats->wrapper_requested_bytes += (uint64_t)bytes;
}
int leaderboard_profile_read(leaderboard_profile_stage_t stage,
                             leaderboard_profile_sample_t *out) {
    if ((unsigned)stage >= LEADERBOARD_PROFILE_STAGE_COUNT || out == NULL ||
        active_count != 0U) return -1;
    *out = measurements[stage];
    return 0;
}
#endif
