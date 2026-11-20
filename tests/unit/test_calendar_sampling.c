/* Deterministic, descriptive sampling diagnostics; NOT a uniformity proof. */
#include "calendar.h"
#include "calendar_test_fixture.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do {if(!(expr)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr);return EXIT_FAILURE;}}while(0)

static int small_exact_reference(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input,4U,3U)==0);
    competition_t *c=NULL;CHECK(competition_create(&input.input,&c)==0);
    calendar_options_t options=calendar_options_default();
    options.method=CALENDAR_METHOD_ROUND_SHUFFLE;
    options.seed=UINT32_C(1301);
    calendar_generator_t *generator=NULL;
    CHECK(calendar_generator_create(c,&options,&generator)==0);
    /* A K4 first cycle has precisely three perfect matchings. Reordering
     * them produces exactly 3!=6 distinct labeled round sequences. */
    enum { SAMPLE_COUNT=6000, POSSIBILITIES=6 };
    uint64_t fingerprints[POSSIBILITIES]={0};
    unsigned count[POSSIBILITIES]={0};
    size_t distinct=0U;
    for(size_t i=0;i<SAMPLE_COUNT;++i) {
        calendar_t *sample=NULL;
        CHECK(calendar_generator_next(generator,&sample,NULL)==0);
        CHECK(calendar_validate(c,sample)==0);
        uint64_t fingerprint=calendar_fingerprint(sample);
        size_t j=0;
        for(;j<distinct;++j) if(fingerprints[j]==fingerprint)break;
        if(j==distinct) {
            CHECK(distinct<POSSIBILITIES);
            fingerprints[distinct++]=fingerprint;
        }
        count[j]++;
        calendar_destroy(sample);
    }
    CHECK(distinct==POSSIBILITIES);
    double chi_square=0.0,expected=(double)SAMPLE_COUNT/(double)POSSIBILITIES;
    for(size_t j=0;j<distinct;++j) {
        double error=(double)count[j]-expected;
        chi_square+=error*error/expected;
    }
    printf("K4 baseline shuffle: %zu/6 distinct, chi-square(5 df)=%.3f over %d samples\n",
           distinct,chi_square,SAMPLE_COUNT);
    /* A wide deterministic smoke bound catches severe selection mistakes.
     * It is NOT a statistical guarantee or a CI performance threshold. */
    CHECK(chi_square<60.0);
    calendar_generator_destroy(generator);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

static int recombination_diversity(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input,8U,14U)==0);
    competition_t *c=NULL;CHECK(competition_create(&input.input,&c)==0);
    calendar_options_t options=calendar_options_default();
    options.method=CALENDAR_METHOD_RECOMBINE;options.seed=UINT32_C(2026);
    options.moves_per_sample=24U;
    calendar_generator_t *generator=NULL;
    CHECK(calendar_generator_create(c,&options,&generator)==0);
    enum { SAMPLES=250 };
    uint64_t seen[SAMPLES];size_t distinct=0U;
    uint64_t partial_moves=0U,three_moves=0U;
    for(size_t i=0;i<SAMPLES;++i) {
        calendar_t *sample=NULL;
        calendar_diagnostics_t stats;
        CHECK(calendar_generator_next(generator,&sample,&stats)==0);
        CHECK(calendar_validate(c,sample)==0);
        uint64_t hash=calendar_fingerprint(sample);
        size_t j=0;
        for(;j<distinct;++j) if(seen[j]==hash)break;
        if(j==distinct) seen[distinct++]=hash;
        partial_moves+=stats.two_round_partial_moves;
        three_moves+=stats.three_round_moves;
        calendar_destroy(sample);
    }
    printf("K8 recombination: %zu/%d distinct observed, %" PRIu64
           " proper 2-round moves, %" PRIu64 " 3-round moves\n",
           distinct,SAMPLES,partial_moves,three_moves);
    CHECK(distinct>1U && partial_moves+three_moves>0U);
    calendar_generator_destroy(generator);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
int main(void) {
    CHECK(small_exact_reference()==0);
    CHECK(recombination_diversity()==0);
    return EXIT_SUCCESS;
}
