/* Public permutation contracts: small leagues, real fixtures, determinism. */
#include "calendar.h"
#include "calendar_test_fixture.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); return EXIT_FAILURE; } } while(0)

static int exercise(size_t n,size_t r,calendar_method_t method,
                    calendar_sampling_mode_t mode) {
    calendar_test_fixture_t fixture;
    CHECK(calendar_test_prepare(&fixture,n,r)==0);
    competition_t *source=NULL;
    CHECK(competition_create(&fixture.input,&source)==0);
    calendar_options_t opts=calendar_options_default();
    opts.method=method;opts.sampling_mode=mode;opts.seed=42U;
    opts.moves_per_sample=30U;opts.chain_warmup_moves=4U;
    opts.chain_thinning_moves=3U;
    calendar_generator_t *first=NULL,*second=NULL;
    CHECK(calendar_generator_create(source,&opts,&first)==0);
    CHECK(calendar_generator_create(source,&opts,&second)==0);
    for(size_t iter=0;iter<15U;++iter) {
        calendar_t *a=NULL,*b=NULL;
        calendar_diagnostics_t da,db;
        CHECK(calendar_generator_next(first,&a,&da)==0 && a!=NULL);
        CHECK(calendar_generator_next(second,&b,&db)==0 && b!=NULL);
        CHECK(calendar_validate(source,a)==0 && calendar_validate(source,b)==0);
        CHECK(calendar_fingerprint(a)==calendar_fingerprint(b));
        CHECK(memcmp(&da,&db,sizeof(da))==0);
        CHECK(calendar_team_count(a)==n && calendar_round_count(a)==r);
        for(size_t i=0;i<r;++i) {
            uint32_t day;
            CHECK(calendar_round_matchday(a,i,&day)==0 && day==i+4U);
            for(size_t j=0;j<n/2U;++j) {
                fixture_t f;
                CHECK(calendar_fixture_at(a,i,j,&f)==0);
                CHECK(f.home<n && f.away<n && f.home!=f.away);
            }
        }
        calendar_destroy(b);
        if(iter==14U) {
            /* Output must be independently owned after generator teardown. */
            calendar_generator_destroy(first);first=NULL;
            CHECK(calendar_validate(source,a)==0);
        }
        calendar_destroy(a);
    }
    calendar_generator_destroy(first);calendar_generator_destroy(second);
    competition_destroy(source);
    return 0;
}

static int reject_incompatible(void) {
    calendar_test_fixture_t f;
    CHECK(calendar_test_prepare(&f,4U,6U)==0);
    f.rounds[1].fixtures=f.rounds[0].fixtures;
    competition_t *source=NULL;
    CHECK(competition_create(&f.input,&source)==0);
    calendar_generator_t *generator=(calendar_generator_t*)(uintptr_t)1U;
    CHECK(calendar_generator_create(source,NULL,&generator)==-ENOTSUP);
    CHECK(generator==NULL);
    competition_destroy(source);
    return 0;
}

/* Import the literal 10-team/35-round user calendar (including pending dates)
 * to check the anchored ninth base round and real directed home/away counts. */
static int real_calendar(void) {
#ifndef CRD_CALENDAR_TSV
#error "Define CRD_CALENDAR_TSV"
#endif
    FILE *file=fopen(CRD_CALENDAR_TSV,"rb");CHECK(file!=NULL);
    char ids[10][TEAM_UUID_TEXT_SIZE];
    char names[10][TEAM_NAME_MAX_BYTES+1];
    competition_team_input_t teams[10];
    competition_fixture_input_t matches[35][5];
    competition_round_input_t rounds[35]={0};
    competition_score_input_t scores[350]={0};
    char line[1024];size_t team_count=0U,fixture_count=0U;
    while(fgets(line,sizeof(line),file)!=NULL) {
        if(line[0]=='#')continue;
        unsigned rd,day,p1,p2;char home[256],away[256],stat;
        CHECK(sscanf(line,"%u\t%u\t%255[^\t]\t%255[^\t]\t%u\t%u\t%c",
                     &rd,&day,home,away,&p1,&p2,&stat)==7);
        CHECK(rd>=1U && rd<=35U && team_count<=10U);
        size_t idx[2];const char *parts[2]={home,away};
        for(size_t k=0;k<2U;++k) {
            size_t j=0;
            for(;j<team_count;++j) if(strcmp(names[j],parts[k])==0) break;
            if(j==team_count) {
                CHECK(team_count<10U);
                (void)snprintf(names[j],sizeof(names[j]),"%s",parts[k]);
                (void)snprintf(ids[j],sizeof(ids[j]),
                               "00000000-0000-4000-8000-%012u",(unsigned)(j+1U));
                teams[j]=(competition_team_input_t){ids[j],names[j]};
                ++team_count;
            }
            idx[k]=j;
        }
        size_t rr=(size_t)(rd-1U), fi=rounds[rr].fixture_count;
        CHECK(fi<5U);
        matches[rr][fi]=(competition_fixture_input_t){ids[idx[0]],ids[idx[1]]};
        rounds[rr].fixture_count++;
        rounds[rr].serie_a_matchday=day;
        CHECK(stat=='F' || stat=='P');
        scores[rr*10U+idx[0]]=(competition_score_input_t){ids[idx[0]],day,
            {p1,stat=='F' ? FANTASY_SCORE_FINAL : FANTASY_SCORE_PENDING}};
        scores[rr*10U+idx[1]]=(competition_score_input_t){ids[idx[1]],day,
            {p2,stat=='F' ? FANTASY_SCORE_FINAL : FANTASY_SCORE_PENDING}};
        fixture_count++;
    }
    CHECK(fclose(file)==0 && fixture_count==175U && team_count==10U);
    for(size_t r=0;r<35U;++r) {
        CHECK(rounds[r].fixture_count==5U);
        rounds[r].fixtures=matches[r];
    }
    competition_input_t input={teams,10U,rounds,35U,scores,350U};
    competition_t *source=NULL;
    CHECK(competition_create(&input,&source)==0);
    CHECK(!competition_is_simulation_ready(source));
    for(unsigned method=0;method<2U;++method) {
        calendar_options_t opts=calendar_options_default();
        opts.seed=12345U;opts.method=(calendar_method_t)method;
        opts.moves_per_sample=80U;
        calendar_generator_t *generator=NULL;
        CHECK(calendar_generator_create(source,&opts,&generator)==0);
        size_t changed=0U,three=0U;
        for(size_t i=0;i<32U;++i) {
            calendar_t *calendar=NULL;calendar_diagnostics_t info;
            CHECK(calendar_generator_next(generator,&calendar,&info)==0);
            CHECK(calendar_validate(source,calendar)==0);
            fixture_t original[5];
            for(size_t j=0;j<5U;++j) CHECK(competition_fixture_at(source,8U,j,&original[j])==0);
            for(size_t j=0;j<5U;++j) {
                fixture_t candidate;int found=0;
                CHECK(calendar_fixture_at(calendar,8U,j,&candidate)==0);
                for(size_t k=0;k<5U;++k)
                    if((candidate.home==original[k].home && candidate.away==original[k].away) ||
                       (candidate.home==original[k].away && candidate.away==original[k].home)) found=1;
                CHECK(found);
            }
            if(info.changed_base_rounds) ++changed;
            three+=(size_t)info.three_round_moves;
            calendar_destroy(calendar);
        }
        CHECK(changed>0U);
        if(method==CALENDAR_METHOD_RECOMBINE) CHECK(three>0U);
        calendar_generator_destroy(generator);
    }
    competition_destroy(source);
    return 0;
}

int main(void) {
    CHECK(calendar_round_count(NULL)==0U && calendar_team_count(NULL)==0U);
    CHECK(calendar_fingerprint(NULL)==0U);
    CHECK(calendar_generator_create(NULL,NULL,NULL)==-EINVAL);
    calendar_t *calendar=(calendar_t*)(uintptr_t)1U;
    CHECK(calendar_generator_next(NULL,&calendar,NULL)==-EINVAL && calendar==NULL);
    calendar_destroy(NULL);calendar_generator_destroy(NULL);
    CHECK(reject_incompatible()==0);
    const size_t sizes[]={2U,4U,8U,10U,12U};
    for(size_t s=0;s<5U;++s) {
        size_t n=sizes[s],p=n-1U;
        size_t rounds[4]={p>1U ? p-1U : 1U,p,p*2U,p*3U-1U};
        for(size_t j=0;j<4U;++j) for(unsigned method=0;method<2U;++method)
            for(unsigned mode=0;mode<2U;++mode)
                CHECK(exercise(n,rounds[j],(calendar_method_t)method,
                               (calendar_sampling_mode_t)mode)==0);
    }
    CHECK(real_calendar()==0);
    return 0;
}
