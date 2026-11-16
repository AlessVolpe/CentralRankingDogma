/* Allocation injection and white-box overflow checks for standings_t. */
#include "standings.h"
#include "standings_internal.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) {                                    \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);       \
    return EXIT_FAILURE;                                              \
} } while (0)

int main(void) {
    const competition_team_input_t teams[] = {
        {"00000000-0000-4000-8000-000000000001", "First"},
        {"00000000-0000-4000-8000-000000000002", "Second"}
    };
    const competition_fixture_input_t match = {teams[0].uuid,teams[1].uuid};
    const competition_round_input_t round = {12U,&match,1U};
    const competition_score_input_t scores[] = {
        {"00000000-0000-4000-8000-000000000001",12U,{0U,FANTASY_SCORE_PENDING}},
        {"00000000-0000-4000-8000-000000000002",12U,{0U,FANTASY_SCORE_PENDING}}
    };
    const competition_input_t input = {teams,2U,&round,1U,scores,2U};
    competition_t *c = NULL;
    CHECK(competition_create(&input,&c) == 0);

    /* Three allocations: owning struct, UUID array, entry array. */
    for (size_t target = 0U; target < 4U; ++target) {
        standings_t *s = (standings_t *)(uintptr_t)1U;
        leaderboard_alloc_fail_after(target);
        int status = standings_create(c,&s);
        leaderboard_alloc_reset();
        if (target < 3U) CHECK(status == -ENOMEM && s == NULL);
        else { CHECK(status == 0 && s != NULL); standings_destroy(s); }
    }

    standings_t *s = NULL;
    CHECK(standings_create(c,&s) == 0);
    competition_destroy(c);
    /* A win for the away team would overflow its valid large score. The
     * home team's loss must NOT be applied (transactional two-entry update). */
    s->entries[1].championship_points = UINT32_MAX;
    s->entries[1].draws = UINT32_MAX; /* 3*0 + UINT32_MAX == UINT32_MAX */
    const standing_entry_t home_before = s->entries[0];
    const standing_entry_t away_before = s->entries[1];
    CHECK(standings_record_match(s,0U,1U,STANDINGS_LOSS) == -EOVERFLOW);
    CHECK(memcmp(&home_before,&s->entries[0],sizeof(home_before)) == 0);
    CHECK(memcmp(&away_before,&s->entries[1],sizeof(away_before)) == 0);

    /* A single-team draw and a single-team win are also failure-safe. */
    CHECK(standings_record_result(s,1U,STANDINGS_DRAW) == -EOVERFLOW);
    CHECK(standings_record_result(s,1U,STANDINGS_WIN) == -EOVERFLOW);
    CHECK(memcmp(&away_before,&s->entries[1],sizeof(away_before)) == 0);

    standings_reset(s);
    s->entries[0].losses = UINT32_MAX;
    const standing_entry_t before = s->entries[0];
    CHECK(standings_record_result(s,0U,STANDINGS_LOSS) == -EOVERFLOW);
    CHECK(memcmp(&before,&s->entries[0],sizeof(before)) == 0);

    standings_reset(s);
    CHECK(standings_record_match(s,0U,1U,STANDINGS_WIN) == 0);
    CHECK(s->entries[0].championship_points == 3U);
    CHECK(s->entries[1].championship_points == 0U);
    standings_destroy(s);
    return EXIT_SUCCESS;
}
