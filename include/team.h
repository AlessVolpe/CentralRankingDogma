/*
 * LeaderboardSystem Fantacalcio team identity (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_TEAM_H
#define LEADERBOARD_TEAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEAM_UUID_TEXT_SIZE 37U
#define TEAM_NAME_MAX_BYTES 255U

typedef struct team_uuid {
    uint8_t bytes[16];
} team_uuid_t;

/* Owned, standalone record. The competition API never exposes these mutable
 * fields; it returns read-only views and copies of identity/score values. */
typedef struct team {
    team_uuid_t id;
    char *name;                  /* allocated and freed by team_destroy */
    uint32_t total_points_tenths; /* sum of FINAL historical fantasy scores */
} team_t;

/* RFC 9562 textual layout (8-4-4-4-12 hex digits), case-insensitive. Parsing
 * does not restrict UUID versions; database UUIDs need not be version 4.
 * 0 = success, -EINVAL = invalid input. */
int team_uuid_parse(const char *text, team_uuid_t *out);
int team_uuid_format(const team_uuid_t *uuid, char out[TEAM_UUID_TEXT_SIZE]);
int team_uuid_compare(const team_uuid_t *lhs, const team_uuid_t *rhs);

/* Deep-copy the name, decode UUID and initialize the season total. The
 * constructor does NOT generate random identifiers. NULL means invalid
 * arguments or out-of-memory; the competition constructor has error codes. */
team_t *team_create(const char *uuid_text, const char *name,
                    uint32_t total_points_tenths);
void team_destroy(team_t *team);

#ifdef __cplusplus
}
#endif
#endif /* LEADERBOARD_TEAM_H */
