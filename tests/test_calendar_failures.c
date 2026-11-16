/* Failure-atomic generator RNG/chain state and adversarial validation. */
#include "calendar.h"
#include "calendar_internal.h"
#include "alloc_internal.h"
#include "calendar_test_fixture.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do {if(!(expr)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr);return EXIT_FAILURE;}}while(0)

static int failure_matrix(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,10U,35U)==0);
    competition_t *c=NULL;CHECK(competition_create(&f.input,&c)==0);
    calendar_options_t options=calendar_options_default();
    options.seed=UINT32_C(0xDEADBEEF);options.moves_per_sample=13U;
    options.sampling_mode=CALENDAR_SAMPLING_CHAIN;
    for(size_t cutoff=0U;cutoff<10U;++cutoff) {
        calendar_generator_t *g=NULL;
        leaderboard_alloc_fail_after(cutoff);
        int status=calendar_generator_create(c,&options,&g);
        leaderboard_alloc_reset();
        CHECK(status==0 || status==-ENOMEM);
        if(status==0)calendar_generator_destroy(g);
        else CHECK(g==NULL);
    }
    /* A successful generator must be unchanged by *any* failed next call. */
    for(size_t cutoff=0U;cutoff<25U;++cutoff) {
        calendar_generator_t *reference=NULL,*candidate=NULL;
        CHECK(calendar_generator_create(c,&options,&reference)==0);
        CHECK(calendar_generator_create(c,&options,&candidate)==0);
        calendar_t *attempt=NULL;
        leaderboard_alloc_fail_after(cutoff);
        int status=calendar_generator_next(candidate,&attempt,NULL);
        leaderboard_alloc_reset();
        CHECK(status==0 || status==-ENOMEM);
        if(status!=0) {
            CHECK(attempt==NULL);
            calendar_t *expected=NULL,*received=NULL;
            CHECK(calendar_generator_next(reference,&expected,NULL)==0);
            CHECK(calendar_generator_next(candidate,&received,NULL)==0);
            CHECK(calendar_fingerprint(expected)==calendar_fingerprint(received));
            calendar_destroy(expected);calendar_destroy(received);
        } else {
            CHECK(attempt!=NULL && calendar_validate(c,attempt)==0);
            calendar_destroy(attempt);
        }
        calendar_generator_destroy(candidate);calendar_generator_destroy(reference);
    }
    competition_destroy(c);return 0;
}

static int validation_edges(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,8U,20U)==0);
    competition_t *competition=NULL;CHECK(competition_create(&f.input,&competition)==0);
    calendar_options_t options=calendar_options_default();
    calendar_generator_t *g=NULL;CHECK(calendar_generator_create(competition,&options,&g)==0);
    calendar_t *c=NULL;CHECK(calendar_generator_next(g,&c,NULL)==0);
    CHECK(calendar_validate(competition,c)==0);
    fixture_t original=c->fixtures[0];
    c->fixtures[0]=c->fixtures[1];
    CHECK(calendar_validate(competition,c)==-EINVAL);
    c->fixtures[0]=original;
    c->matchdays[0]++;
    CHECK(calendar_validate(competition,c)==-EINVAL);
    c->matchdays[0]--;
    /* Intact source and candidate must both remain valid. */
    CHECK(calendar_validate(competition,c)==0);
    CHECK(calendar_validate(NULL,c)==-EINVAL);
    CHECK(calendar_validate(competition,NULL)==-EINVAL);
    /* A changed directed match that keeps the undirected pair still fails. */
    c->fixtures[0]=(fixture_t){original.away,original.home};
    CHECK(calendar_validate(competition,c)==-EINVAL);
    c->fixtures[0]=original;
    CHECK(calendar_validate(competition,c)==0);
    /* Public API argument and bounds validation. */
    CHECK(calendar_fixture_at(c,999U,0U,&original)==-ERANGE);
    CHECK(calendar_round_matchday(c,999U,NULL)==-EINVAL);
    CHECK(calendar_round_matchday(c,999U,&(uint32_t){0})==-ERANGE);
    calendar_destroy(c);
    calendar_generator_destroy(g);
    options.method=(calendar_method_t)99;
    CHECK(calendar_generator_create(competition,&options,&g)==-EINVAL && g==NULL);
    options=calendar_options_default();options.three_round_node_limit=0U;
    CHECK(calendar_generator_create(competition,&options,&g)==-EINVAL && g==NULL);
    options=calendar_options_default();options.three_round_node_limit=1U;
    CHECK(calendar_generator_create(competition,&options,&g)==0);
    CHECK(calendar_generator_next(g,&c,NULL)==0);
    CHECK(calendar_validate(competition,c)==0);
    calendar_destroy(c);calendar_generator_destroy(g);
    competition_destroy(competition);return 0;
}
int main(void) {
    CHECK(failure_matrix()==0);
    CHECK(validation_edges()==0);
    return 0;
}
