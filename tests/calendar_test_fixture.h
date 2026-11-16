/* Deterministic round-robin input builder for 3.0.12 tests. */
#ifndef LEADERBOARD_CALENDAR_TEST_FIXTURE_H
#define LEADERBOARD_CALENDAR_TEST_FIXTURE_H
#include "competition.h"
#include <stdio.h>
#include <string.h>

typedef struct calendar_test_fixture {
    char ids[12][TEAM_UUID_TEXT_SIZE];
    competition_team_input_t teams[12];
    competition_fixture_input_t matches[40][6];
    competition_round_input_t rounds[40];
    competition_score_input_t scores[480];
    competition_input_t input;
} calendar_test_fixture_t;

static int calendar_test_prepare(calendar_test_fixture_t *fixture,
                                 size_t n,size_t r) {
    if(n<2U || n>12U || (n&1U)!=0U || r==0U || r>40U) return -1;
    memset(fixture,0,sizeof(*fixture));
    for(size_t i=0;i<n;++i) {
        (void)snprintf(fixture->ids[i],TEAM_UUID_TEXT_SIZE,
                       "00000000-0000-4000-8000-%012u",(unsigned)(i+1U));
        fixture->teams[i]=(competition_team_input_t){fixture->ids[i],"A team"};
    }
    size_t arrangement[12];for(size_t i=0;i<n;++i)arrangement[i]=i;
    size_t period=n-1U;
    for(size_t round=0;round<r;++round) {
        size_t k=n/2U;
        for(size_t i=0;i<k;++i) {
            size_t a=arrangement[i],b=arrangement[n-1U-i];
            if((round/period)%2U != 0U) {size_t tmp=a;a=b;b=tmp;}
            fixture->matches[round][i]=(competition_fixture_input_t){
                fixture->ids[a],fixture->ids[b]
            };
        }
        fixture->rounds[round]=(competition_round_input_t){
            (uint32_t)(round+4U),fixture->matches[round],k
        };
        for(size_t i=0;i<n;++i) {
            fixture->scores[round*n+i]=(competition_score_input_t){
                fixture->ids[i],(uint32_t)(round+4U),
                {round<8U ? (uint32_t)(600U+10U*i+round) : 0U,
                 round<8U ? FANTASY_SCORE_FINAL : FANTASY_SCORE_PENDING}
            };
        }
        /* Circle method: hold team 0 fixed, rotate the other N-1 teams. */
        size_t last=arrangement[n-1U];
        for(size_t i=n-1U;i>1U;--i) arrangement[i]=arrangement[i-1U];
        arrangement[1]=last;
    }
    fixture->input=(competition_input_t){fixture->teams,n,fixture->rounds,r,
                                         fixture->scores,n*r};
    return 0;
}
#endif
