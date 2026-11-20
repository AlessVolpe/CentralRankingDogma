/*
 * Central Ranking Dogma CLI example (legacy).
 * Copyright (C) 2024-2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bostree.h"
#include "commons.h"
#include "hashmap_typed.h"
#include "mtwister.h"
#include "team.h"

typedef struct demo_standing {
    team_t *team;
    uint32_t championship_points;
} demo_standing_t;

HASHMAP_TYPED_DEFINE(player_pool, char, demo_standing_t)

/* The legacy CLI alone retains its short interactive input limit. */
#define LEGACY_CLI_NAME_CAP 25U

/* Team names are deep-copied; the tree owns its demo_standing_t values. */
static void _tree_free(BOSNode *node) {
    demo_standing_t *entry = bostree_node_data(node);
    team_destroy(entry->team);
    free(entry);
}

static int _compare_names(const void *lhs, const void *rhs) {
    return strcmp(lhs, rhs);
}

static int _select_players(void) {
    int count;
    printf("Insert player number: \n");
    if (scanf("%d", &count) != 1 || count < MIN_PLAYERS || count > MAX_PLAYERS) {
        fprintf(stderr, "Expected a player count between %d and %d.\n", MIN_PLAYERS, MAX_PLAYERS);
        return -1;
    }
    return count;
}

static void _free_player_names(size_t count, char **names) {
    if (names == NULL) return;
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

static char **_name_players(size_t count) {
    char **names = calloc(count, sizeof(*names));
    if (names == NULL) return NULL;

    /* scanf() used for the count leaves its newline in stdin. */
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {}

    for (size_t i = 0; i < count; ++i) {
        names[i] = malloc(LEGACY_CLI_NAME_CAP);
        if (names[i] == NULL) {
            _free_player_names(count, names);
            return NULL;
        }
        printf("Insert team name: \n");
        /* fgets preserves spaces in team names (e.g. Brigate Ebosse).
         * Detect and reject overlong lines rather than treating the remainder
         * as the next team's name. */
        char line[LEGACY_CLI_NAME_CAP + 2U];
        if (fgets(line, sizeof(line), stdin) == NULL) {
            _free_player_names(count, names);
            return NULL;
        }
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
        } else if (!feof(stdin)) {
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            _free_player_names(count, names);
            return NULL;
        }
        size_t len = strlen(line);
        if (len == 0U || len >= LEGACY_CLI_NAME_CAP) {
            _free_player_names(count, names);
            return NULL;
        }
        memcpy(names[i], line, len + 1U);
    }
    return names;
}

int main(void) {
    const int requested = _select_players();
    if (requested < 0) return EXIT_FAILURE;

    const size_t count = (size_t)requested;
    char **names = _name_players(count);
    if (names == NULL) {
        fprintf(stderr, "Unable to read or allocate player names.\n");
        return EXIT_FAILURE;
    }

    /* The hashmap borrows UUID keys; ids[] retains ownership until cleanup. */
    char *ids[MAX_PLAYERS] = {NULL};
    player_pool_t pool = {0};
    if (player_pool_init(&pool, hashmap_hash_string, strcmp) != 0) {
        fprintf(stderr, "Unable to initialize player hashmap.\n");
        _free_player_names(count, names);
        return EXIT_FAILURE;
    }

    BOSTree *leaderboard = bostree_new(_compare_names, _tree_free);
    MTRand *random = seedRand((uint32_t)time(NULL));
    int result = EXIT_FAILURE;

    if (leaderboard == NULL || random == NULL) {
        fprintf(stderr, "Unable to initialize leaderboard or random generator.\n");
        goto cleanup;
    }

    for (size_t i = 0; i < count; ++i) {
        char *id = uuid_generate();
        /* Synthetic season aggregate is fixed for this demo fixture.
         * The real Python orchestrator will supply recorded season totals. */
        team_t *team = id == NULL ? NULL : team_create(
            id, names[i], UINT32_C(20000) + genRandLong(random) % UINT32_C(10000));
        demo_standing_t *entry = team == NULL ? NULL : malloc(sizeof(*entry));
        if (id == NULL || team == NULL || entry == NULL) {
            free(id);
            team_destroy(team);
            free(entry);
            fprintf(stderr, "Unable to allocate player.\n");
            goto cleanup;
        }

        entry->team = team;
        entry->championship_points = 0U;
        const int status = player_pool_put(&pool, id, entry);
        if (status != 0) {
            fprintf(stderr, "Unable to store player %s (error %d).\n", names[i], status);
            team_destroy(team);
            free(entry);
            free(id);
            goto cleanup;
        }
        ids[i] = id;
        printf("Added player %s [%s].\n", team->name, id);
    }

    printf("\nFormat: team_name[team_id], championship points,"
           " fixed season total\n");
    player_pool_iterator_t it = player_pool_iter(&pool);
    while (player_pool_iter_valid(&it)) {
        const char *key = player_pool_iter_key(&it);
        demo_standing_t *data = player_pool_iter_value(&it);
        /* One illustrative match result: win (3), draw (1), loss (0). */
        const uint32_t outcome = genRandLong(random) % 3U;
        const uint32_t award = outcome == 0U ? 3U : (outcome == 1U ? 1U : 0U);
        /* Demonstration only. Match-result calculation belongs to 3.1.0. */
        data->championship_points += award;
        printf("Team: %s[%s] with %" PRIu32
               " championship points; season fantasy total %" PRIu32 ".%" PRIu32 "\n",
               data->team->name, key, data->championship_points,
               data->team->total_points_tenths / 10U,
               data->team->total_points_tenths % 10U);
        (void)player_pool_iter_next(&it);
    }

    /* Removing transfers player ownership from the pool to the tree. */
    it = player_pool_iter(&pool);
    while (player_pool_iter_valid(&it)) {
        demo_standing_t *entry = player_pool_iter_value(&it);
        (void)player_pool_iter_remove(&it);
        BOSNode *node = bostree_insert(leaderboard, entry->team->name, entry);
        if (node == NULL) {
            team_destroy(entry->team);
            free(entry);
            fprintf(stderr, "Unable to insert leaderboard node.\n");
            goto cleanup;
        }
        if (bostree_node_data(node) != entry) {
            /* Comparator-equal names share a ranking entry. */
            printf("Duplicate player name %s ignored.\n", entry->team->name);
            team_destroy(entry->team);
            free(entry);
        } else {
            printf("Inserted player %s with %" PRIu32 " points.\n", entry->team->name, entry->championship_points);
        }
    }

#ifndef NDEBUG
    bostree_print(leaderboard);
#endif
    result = EXIT_SUCCESS;

cleanup:
    /* These are the values still in the hashmap, not the ones in the tree. */
    it = player_pool_iter(&pool);
    while (player_pool_iter_valid(&it)) {
        demo_standing_t *entry = player_pool_iter_value(&it);
        (void)player_pool_iter_remove(&it);
        team_destroy(entry->team);
        free(entry);
    }
    player_pool_cleanup(&pool);
    bostree_destroy(leaderboard);
    for (size_t i = 0; i < count; ++i) free(ids[i]);
    _free_player_names(count, names);
    free(random);
    return result;
}
