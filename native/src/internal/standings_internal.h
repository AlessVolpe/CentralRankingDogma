/*
 * LeaderboardSystem standings private representation (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 * Used only by the implementation and designated white-box tests.
 */
#ifndef LEADERBOARD_STANDINGS_INTERNAL_H
#define LEADERBOARD_STANDINGS_INTERNAL_H

#include "standings.h"

struct standings {
    size_t team_count;
    team_uuid_t *ids;       /* UUID-sorted, private copy */
    standing_entry_t *entries;
    standings_goal_stats_t *goals;
};

#endif /* LEADERBOARD_STANDINGS_INTERNAL_H */
