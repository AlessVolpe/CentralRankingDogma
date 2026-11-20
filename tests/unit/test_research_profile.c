/*
 * LeaderboardSystem internal research instrumentation smoke tests (3.1.2).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "research_profile_internal.h"
#include "alloc_internal.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) {                                         \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);                \
    return EXIT_FAILURE;                                                       \
} } while (0)

int main(void) {
    leaderboard_profile_sample_t outer, inner;
    leaderboard_profile_reset();
    leaderboard_profile_begin(LEADERBOARD_PROFILE_RESOLUTION);
    void *a = leaderboard_alloc_malloc(32U);
    CHECK(a != NULL);
    leaderboard_profile_begin(LEADERBOARD_PROFILE_RANKING);
    void *b = leaderboard_alloc_calloc(4U, 16U);
    CHECK(b != NULL);
    leaderboard_profile_end(LEADERBOARD_PROFILE_RANKING);
    leaderboard_profile_end(LEADERBOARD_PROFILE_RESOLUTION);
    CHECK(leaderboard_profile_read(LEADERBOARD_PROFILE_RESOLUTION, &outer) == 0);
    CHECK(leaderboard_profile_read(LEADERBOARD_PROFILE_RANKING, &inner) == 0);
    CHECK(outer.calls == 1U && inner.calls == 1U);
    CHECK(outer.wrapper_allocations == 1U && outer.wrapper_requested_bytes == 32U);
    CHECK(inner.wrapper_allocations == 1U && inner.wrapper_requested_bytes == 64U);
    CHECK(outer.wall_seconds >= 0.0 && inner.wall_seconds >= 0.0);
    CHECK(outer.cpu_seconds >= 0.0 && inner.cpu_seconds >= 0.0);
    free(a);
    free(b);
    leaderboard_profile_reset();
    CHECK(leaderboard_profile_read(LEADERBOARD_PROFILE_RANKING, &inner) == 0);
    CHECK(inner.calls == 0U && inner.wrapper_allocations == 0U);
    return EXIT_SUCCESS;
}
