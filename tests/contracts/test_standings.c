/* Public championship standings contract (milestone 3.0.11). */
#include "competition.h"
#include "standings.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) {                                     \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);        \
    return EXIT_FAILURE;                                               \
} } while (0)

static const char *id1 = "00000000-0000-4000-8000-000000000001";
static const char *id2 = "00000000-0000-4000-8000-000000000002";
static const char *id3 = "00000000-0000-4000-8000-000000000003";
static const char *id4 = "00000000-0000-4000-8000-000000000004";

static int test_standings(void) {
    const competition_team_input_t teams[] = {
        {"00000000-0000-4000-8000-000000000003", "Same name"},
        {"00000000-0000-4000-8000-000000000001", "Same name"},
        {"00000000-0000-4000-8000-000000000004", "Four"},
        {"00000000-0000-4000-8000-000000000002", "Two"}
    };
    const competition_fixture_input_t fixtures[] = {
        {"00000000-0000-4000-8000-000000000003", "00000000-0000-4000-8000-000000000001"},
        {"00000000-0000-4000-8000-000000000002", "00000000-0000-4000-8000-000000000004"}
    };
    const competition_round_input_t rounds[] = {{12U,fixtures,2U}};
    const competition_score_input_t scores[] = {
        {"00000000-0000-4000-8000-000000000004",12U,{0U,FANTASY_SCORE_PENDING}},
        {"00000000-0000-4000-8000-000000000001",12U,{0U,FANTASY_SCORE_PENDING}},
        {"00000000-0000-4000-8000-000000000002",12U,{0U,FANTASY_SCORE_PENDING}},
        {"00000000-0000-4000-8000-000000000003",12U,{0U,FANTASY_SCORE_PENDING}}
    };
    const competition_input_t input = {teams,4U,rounds,1U,scores,4U};
    competition_t *competition = NULL;
    CHECK(competition_create(&input,&competition) == 0);
    CHECK(!competition_is_simulation_ready(competition));

    standings_t *s = (standings_t *)(uintptr_t)1U;
    CHECK(standings_create(NULL,&s) == -EINVAL && s == NULL);
    CHECK(standings_create(competition,NULL) == -EINVAL);
    CHECK(standings_create(competition,&s) == 0 && s != NULL);
    CHECK(standings_team_count(s) == 4U);

    team_index_t idx = UINT32_MAX;
    CHECK(standings_index_for_uuid(s,id1,&idx) == 0 && idx == 0U);
    CHECK(standings_index_for_uuid(s,id4,&idx) == 0 && idx == 3U);
    CHECK(standings_index_for_uuid(s,"00000000-0000-4000-8000-000000000005",&idx) == -ENOENT);
    CHECK(standings_index_for_uuid(s,"not-a-uuid",&idx) == -EINVAL);
    CHECK(standings_index_for_uuid(s,id1,NULL) == -EINVAL);
    CHECK(standings_index_for_uuid(NULL,id1,&idx) == -EINVAL);
    CHECK(standings_entry_for_uuid(s,id1,NULL) == -EINVAL);
    CHECK(standings_entry_for_uuid(s,"wrong", &(standing_entry_t){0}) == -EINVAL);

    /* The table is self-contained, even after the original snapshot dies. */
    competition_destroy(competition);
    standing_entry_t row;
    CHECK(standings_entry_at(s,0U,&row) == 0);
    CHECK(row.team_index == 0U && row.championship_points == 0U);
    CHECK(row.wins == 0U && row.draws == 0U && row.losses == 0U);

    CHECK(standings_record_match(s,0U,1U,STANDINGS_WIN) == 0);
    CHECK(standings_record_match(s,2U,3U,STANDINGS_DRAW) == 0);
    CHECK(standings_record_match(s,1U,0U,STANDINGS_WIN) == 0);
    CHECK(standings_record_result(s,0U,STANDINGS_LOSS) == 0);
    CHECK(standings_entry_at(s,0U,&row) == 0);
    CHECK(row.wins == 1U && row.draws == 0U && row.losses == 2U);
    CHECK(row.championship_points == 3U);
    CHECK(standings_entry_for_uuid(s,id2,&row) == 0);
    CHECK(row.team_index == 1U && row.wins == 1U && row.losses == 1U);
    CHECK(row.championship_points == 3U);
    CHECK(standings_entry_for_uuid(s,id3,&row) == 0);
    CHECK(row.draws == 1U && row.championship_points == 1U);
    CHECK(standings_entry_for_uuid(s,id4,&row) == 0);
    CHECK(row.draws == 1U && row.championship_points == 1U);

    CHECK(standings_record_result(s,4U,STANDINGS_WIN) == -ERANGE);
    CHECK(standings_record_result(NULL,0U,STANDINGS_WIN) == -EINVAL);
    CHECK(standings_record_result(s,0U,(standings_result_t)2) == -EINVAL);
    CHECK(standings_record_match(NULL,0U,1U,STANDINGS_WIN) == -EINVAL);
    CHECK(standings_record_match(s,0U,0U,STANDINGS_DRAW) == -EINVAL);
    CHECK(standings_record_match(s,0U,4U,STANDINGS_DRAW) == -ERANGE);
    CHECK(standings_record_match(s,0U,1U,(standings_result_t)2) == -EINVAL);
    CHECK(standings_entry_at(s,4U,&row) == -ERANGE);
    CHECK(standings_entry_at(s,0U,NULL) == -EINVAL);

    /* Invalid calls must not increment any counter. */
    CHECK(standings_entry_for_uuid(s,id1,&row) == 0);
    CHECK(row.wins == 1U && row.draws == 0U && row.losses == 2U);
    CHECK(row.championship_points == 3U);

    standings_reset(s);
    for (team_index_t i = 0U; i < 4U; ++i) {
        CHECK(standings_entry_at(s,i,&row) == 0);
        CHECK(row.team_index == i && row.championship_points == 0U);
        CHECK(row.wins == 0U && row.draws == 0U && row.losses == 0U);
    }
    CHECK(standings_record_match(s,0U,1U,STANDINGS_LOSS) == 0);
    CHECK(standings_entry_at(s,0U,&row) == 0 && row.losses == 1U && row.championship_points == 0U);
    CHECK(standings_entry_at(s,1U,&row) == 0 && row.wins == 1U && row.championship_points == 3U);

    standings_destroy(s);
    standings_destroy(NULL);
    standings_reset(NULL);
    CHECK(standings_team_count(NULL) == 0U);
    return EXIT_SUCCESS;
}

int main(void) { return test_standings(); }
