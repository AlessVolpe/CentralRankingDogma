/* 3.0.11: every allocation site must roll back without publishing a snapshot. */
#include "competition.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return EXIT_FAILURE;} } while (0)
int main(void) {
    const competition_team_input_t teams[2]={
      {"00000000-0000-4000-8000-000000000002","Brigate Ebosse"},
      {"00000000-0000-4000-8000-000000000001","Boh"}
    };
    const competition_fixture_input_t match={teams[0].uuid,teams[1].uuid};
    const competition_round_input_t round={12U,&match,1U};
    const competition_score_input_t scores[2]={
      {"00000000-0000-4000-8000-000000000002",12U,{0U,FANTASY_SCORE_PENDING}},
      {"00000000-0000-4000-8000-000000000001",12U,{0U,FANTASY_SCORE_PENDING}}
    };
    const competition_input_t input={teams,2U,&round,1U,scores,2U};
    size_t failures=0U;
    size_t successes=0U;
    for (size_t target=0U;target<64U;target++) {
        competition_t *c=(competition_t*)(uintptr_t)1U;
        leaderboard_alloc_fail_after(target);
        int status=competition_create(&input,&c);
        leaderboard_alloc_reset();
        if (status==0) { CHECK(c!=NULL);competition_destroy(c);successes++; }
        else { CHECK(status==-ENOMEM && c==NULL);failures++; }
    }
    CHECK(failures>=8U && successes>=1U);
    /* Allocation injection during standalone team construction. */
    for (size_t target=0U;target<2U;target++) {
        leaderboard_alloc_fail_after(target);
        team_t *t=team_create(teams[0].uuid,teams[0].name,0U);
        leaderboard_alloc_reset();
        CHECK(t==NULL);
    }
    return EXIT_SUCCESS;
}
