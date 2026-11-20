/*
 * LeaderboardSystem Fantacalcio team identity (milestone 3.0.11).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "team.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int team_uuid_parse(const char *text, team_uuid_t *out) {
    if (text == NULL || out == NULL) return -EINVAL;
    team_uuid_t parsed = {{0}};
    size_t at = 0U;
    for (size_t i = 0U; i < 36U; ++i) {
        if (text[i] == '\0') return -EINVAL;
        if (i == 8U || i == 13U || i == 18U || i == 23U) {
            if (text[i] != '-') return -EINVAL;
        } else {
            int nibble = hex_value(text[i]);
            if (nibble < 0) return -EINVAL;
            if ((at & 1U) == 0U) parsed.bytes[at / 2U] = (uint8_t)(nibble << 4);
            else parsed.bytes[at / 2U] |= (uint8_t)nibble;
            ++at;
        }
    }
    if (text[36] != '\0') return -EINVAL;
    *out = parsed;
    return 0;
}

int team_uuid_format(const team_uuid_t *uuid, char out[TEAM_UUID_TEXT_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    if (uuid == NULL || out == NULL) return -EINVAL;
    size_t k = 0U;
    for (size_t i = 0U; i < 16U; ++i) {
        if (i == 4U || i == 6U || i == 8U || i == 10U) out[k++] = '-';
        out[k++] = digits[uuid->bytes[i] >> 4];
        out[k++] = digits[uuid->bytes[i] & 15U];
    }
    out[k] = '\0';
    return 0;
}

int team_uuid_compare(const team_uuid_t *lhs, const team_uuid_t *rhs) {
    if (lhs == NULL || rhs == NULL) return 0; /* both operands required */
    return memcmp(lhs->bytes, rhs->bytes, sizeof(lhs->bytes));
}

/* Bounded scan avoids unbounded strlen for correctly terminated long names.
 * All char* arguments must still point to readable NUL-terminated storage. */
static size_t bounded_name_length(const char *name) {
    if (name == NULL) return 0U;
    size_t n = 0U;
    while (n <= TEAM_NAME_MAX_BYTES && name[n] != '\0') ++n;
    if (n == 0U || n > TEAM_NAME_MAX_BYTES) return 0U;
    return n;
}

team_t *team_create(const char *uuid_text, const char *name,
                    uint32_t total_points_tenths) {
    team_uuid_t uuid;
    if (team_uuid_parse(uuid_text, &uuid) != 0) return NULL;
    size_t length = bounded_name_length(name);
    if (length == 0U) return NULL;
    team_t *team = leaderboard_alloc_calloc(1U, sizeof(*team));
    if (team == NULL) return NULL;
    team->name = leaderboard_alloc_malloc(length + 1U);
    if (team->name == NULL) { free(team); return NULL; }
    memcpy(team->name, name, length + 1U);
    team->id = uuid;
    team->total_points_tenths = total_points_tenths;
    return team;
}

void team_destroy(team_t *team) {
    if (team == NULL) return;
    free(team->name);
    free(team);
}
