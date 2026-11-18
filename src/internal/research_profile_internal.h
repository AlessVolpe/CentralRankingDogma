/*
 * LeaderboardSystem opt-in research-stage instrumentation, internal only.
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 * NOT an installed public API. Do not use in multithreaded library calls.
 */
#ifndef LEADERBOARD_RESEARCH_PROFILE_INTERNAL_H
#define LEADERBOARD_RESEARCH_PROFILE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef enum leaderboard_profile_stage {
    LEADERBOARD_PROFILE_GENERATION = 0,
    LEADERBOARD_PROFILE_RESOLUTION = 1,
    LEADERBOARD_PROFILE_RANKING = 2,
    LEADERBOARD_PROFILE_AGGREGATION = 3,
    LEADERBOARD_PROFILE_UNIQUENESS = 4,
    LEADERBOARD_PROFILE_STAGE_COUNT = 5
} leaderboard_profile_stage_t;

typedef struct leaderboard_profile_sample {
    uint64_t calls;
    double wall_seconds;
    double cpu_seconds;
    uint64_t wrapper_allocations;
    uint64_t wrapper_requested_bytes;
} leaderboard_profile_sample_t;

#ifdef LEADERBOARD_RESEARCH_PROFILE
void leaderboard_profile_reset(void);
void leaderboard_profile_begin(leaderboard_profile_stage_t stage);
void leaderboard_profile_end(leaderboard_profile_stage_t stage);
void leaderboard_profile_record_wrapper_allocation(size_t bytes);
int leaderboard_profile_read(leaderboard_profile_stage_t stage,
                             leaderboard_profile_sample_t *out);
#define LB_PROFILE_BEGIN(stage) leaderboard_profile_begin(stage)
#define LB_PROFILE_END(stage) leaderboard_profile_end(stage)
#else
#define LB_PROFILE_BEGIN(stage) ((void)0)
#define LB_PROFILE_END(stage) ((void)0)
#endif

#endif /* LEADERBOARD_RESEARCH_PROFILE_INTERNAL_H */
