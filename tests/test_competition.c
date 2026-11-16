/* 3.0.11 competition contracts and user-provided real fixture regression. */
#include "competition.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); return EXIT_FAILURE; } } while (0)

static const char *U0="00000000-0000-4000-8000-000000000001";
static const char *U1="00000000-0000-4000-8000-000000000002";
static const competition_team_input_t basic_teams[] = {
    {"00000000-0000-4000-8000-000000000002","Second team"},
    {"00000000-0000-4000-8000-000000000001","First team"}
};
static const competition_fixture_input_t basic_matches[2][1] = {
    {{"00000000-0000-4000-8000-000000000001", "00000000-0000-4000-8000-000000000002"}},
    {{"00000000-0000-4000-8000-000000000002", "00000000-0000-4000-8000-000000000001"}}
};
static const competition_round_input_t basic_rounds[] = {
    {4U, basic_matches[0], 1U}, {6U,basic_matches[1],1U}
};
static const competition_score_input_t basic_scores[] = {
    {"00000000-0000-4000-8000-000000000001",4U,{745U,FANTASY_SCORE_FINAL}},
    {"00000000-0000-4000-8000-000000000002",4U,{680U,FANTASY_SCORE_FINAL}},
    {"00000000-0000-4000-8000-000000000001",6U,{0U,FANTASY_SCORE_PENDING}},
    {"00000000-0000-4000-8000-000000000002",6U,{0U,FANTASY_SCORE_PENDING}}
};
static competition_input_t basic(void) {
    competition_input_t in={basic_teams,2U,basic_rounds,2U,basic_scores,4U};
    return in;
}
static int contract(void) {
    competition_t *c=(competition_t*)(uintptr_t)1U;
    CHECK(competition_create(NULL,&c)==-EINVAL && c==NULL);
    CHECK(competition_create(NULL,NULL)==-EINVAL);
    competition_input_t in=basic();
    CHECK(competition_create(&in,&c)==0 && c!=NULL);
    CHECK(competition_team_count(c)==2U && competition_round_count(c)==2U);
    CHECK(!competition_is_simulation_ready(c));
    team_index_t a,b;
    CHECK(competition_team_index(c,U0,&a)==0 && a==0U);
    CHECK(competition_team_index(c,U1,&b)==0 && b==1U);
    CHECK(competition_team_index(c,"11111111-1111-4111-8111-111111111111",&b)==-ENOENT);
    fixture_t fixture;
    CHECK(competition_fixture_at(c,0,0,&fixture)==0 && fixture.home==0U && fixture.away==1U);
    CHECK(competition_fixture_at(c,1,0,&fixture)==0 && fixture.home==1U && fixture.away==0U);
    CHECK(competition_fixture_at(c,2,0,&fixture)==-ERANGE);
    uint32_t count,matchday,total;const char *name;team_uuid_t uid;bool complete;
    CHECK(competition_pair_count(c,0,1,&count)==0 && count==2U);
    CHECK(competition_pair_count(c,0,0,&count)==-ERANGE);
    CHECK(competition_round_matchday(c,1,&matchday)==0 && matchday==6U);
    CHECK(competition_team_at(c,0,&uid,&name,&total,&complete)==0);
    CHECK(strcmp(name,"First team")==0 && total==745U && !complete);
    fantasy_score_t score;
    CHECK(competition_score_at(c,0,0,&score)==0 && score.status==FANTASY_SCORE_FINAL && score.points_tenths==745U);
    CHECK(competition_score_at(c,0,1,&score)==0 && score.status==FANTASY_SCORE_PENDING);
    competition_destroy(c);

    /* Pending is distinct from the valid finalized value zero. */
    competition_score_input_t scores[4];memcpy(scores,basic_scores,sizeof(scores));
    scores[2].score.status=FANTASY_SCORE_FINAL;
    scores[3].score.status=FANTASY_SCORE_FINAL;
    in.scores=scores;
    CHECK(competition_create(&in,&c)==0 && competition_is_simulation_ready(c));
    CHECK(competition_team_at(c,0,&uid,&name,&total,&complete)==0 && complete && total==745U);
    competition_destroy(c);

    /* Duplicate UUID after case-folding, same names allowed, unknown ID rejected. */
    competition_team_input_t teams[2];memcpy(teams,basic_teams,sizeof(teams));
    teams[0].uuid="00000000-0000-4000-8000-000000000001";
    in=basic();in.teams=teams; c=(competition_t*)(uintptr_t)1;
    CHECK(competition_create(&in,&c)==-EEXIST && c==NULL);
    teams[0]=basic_teams[0];teams[1].name=teams[0].name;
    CHECK(competition_create(&in,&c)==0);competition_destroy(c);

    /* Unknown opponent and duplicate participation. */
    competition_fixture_input_t bad_fixture={0};
    bad_fixture.home_uuid=U0; bad_fixture.away_uuid=U0;
    competition_round_input_t rounds[2];memcpy(rounds,basic_rounds,sizeof(rounds));
    rounds[0].fixtures=&bad_fixture;
    in=basic();in.rounds=rounds;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    bad_fixture.away_uuid="99999999-9999-4999-8999-999999999999";
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);

    in=basic();rounds[0]=basic_rounds[0];rounds[1]=basic_rounds[1];
    rounds[1].serie_a_matchday=4U;in.rounds=rounds;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    rounds[1].serie_a_matchday=3U;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);

    in=basic();in.score_count=3U;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    in=basic();memcpy(scores,basic_scores,sizeof(scores));scores[3]=scores[2];in.scores=scores;
    CHECK(competition_create(&in,&c)==-EEXIST && c==NULL);
    scores[3]=basic_scores[3];scores[3].score.points_tenths=1U;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    scores[3].score.points_tenths=0U;scores[3].score.status=(fantasy_score_status_t)3;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    /* The derived total must not wrap uint32_t. */
    memcpy(scores,basic_scores,sizeof(scores));
    scores[0].score.points_tenths=UINT32_MAX;
    scores[2].score.points_tenths=1U;
    scores[2].score.status=FANTASY_SCORE_FINAL;
    CHECK(competition_create(&in,&c)==-EOVERFLOW && c==NULL);

    in=basic();in.team_count=1U;
    CHECK(competition_create(&in,&c)==-EINVAL && c==NULL);
    competition_destroy(NULL);
    return EXIT_SUCCESS;
}

/* Also exercise 4, 8, and 12-team public profiles with minimal rounds. */
static int typical_league_sizes(void) {
    for (size_t n=4U;n<=12U;n+=4U) {
        char ids[12][TEAM_UUID_TEXT_SIZE];
        competition_team_input_t teams[12];
        competition_score_input_t scores[12];
        competition_fixture_input_t fixtures[6];
        for (size_t i=0U;i<n;i++) {
            (void)snprintf(ids[i],sizeof(ids[i]),
                           "00000000-0000-4000-8000-%012u",(unsigned)(i+1U));
            teams[i]=(competition_team_input_t){ids[i],"A team"};
            scores[i]=(competition_score_input_t){ids[i],12U,{0U,FANTASY_SCORE_FINAL}};
        }
        for (size_t i=0U;i<n/2U;i++) {
            fixtures[i]=(competition_fixture_input_t){ids[2U*i],ids[2U*i+1U]};
        }
        competition_round_input_t round={12U,fixtures,n/2U};
        competition_input_t input={teams,n,&round,1U,scores,n};
        competition_t *c=NULL;
        CHECK(competition_create(&input,&c)==0 && c!=NULL);
        CHECK(competition_is_simulation_ready(c) && competition_team_count(c)==n);
        for (team_index_t i=0U;i<n;i+=2U) {
            uint32_t k;
            CHECK(competition_pair_count(c,i,i+1U,&k)==0 && k==1U);
        }
        competition_destroy(c);
        /* Duplicate participation in a four-or-more-team round is invalid. */
        fixtures[1]=fixtures[0];
        CHECK(competition_create(&input,&c)==-EINVAL && c==NULL);
    }
    return EXIT_SUCCESS;
}

/* Read-only fixture regression exported from the real 2026/27 calendar.
 * Test UUIDs below are deterministic LOCAL FIXTURE IDs, not database IDs. */
static char names[10][TEAM_NAME_MAX_BYTES+1];
static char uuids[10][TEAM_UUID_TEXT_SIZE];
static competition_team_input_t teams[10];
static competition_fixture_input_t fixture_inputs[35][5];
static competition_round_input_t rounds[35];
static competition_score_input_t scores[350];
static int name_index(const char *name, int *registered) {
    for (int i=0;i<*registered;i++) if (strcmp(names[i],name)==0) return i;
    if (*registered>=10) return -1;
    int idx=(*registered)++;
    (void)snprintf(names[idx],sizeof(names[idx]),"%s",name);
    (void)snprintf(uuids[idx],sizeof(uuids[idx]),
                   "00000000-0000-4000-8000-%012u",(unsigned)(idx+1));
    teams[idx]=(competition_team_input_t){uuids[idx],names[idx]};
    return idx;
}
static int regression(void) {
#ifndef LEADERBOARD_CALENDAR_TSV
#error "Set path to user-provided calendar fixture"
#endif
    FILE *file=fopen(LEADERBOARD_CALENDAR_TSV,"rb");
    CHECK(file!=NULL);
    char line[1024];int registered=0,fixture_lines=0,final_fixture_count=0;
    unsigned char seen[350]={0};
    while (fgets(line,sizeof(line),file)!=NULL) {
        if (line[0]=='#') continue;
        unsigned round,day,home_score,away_score;char home[256],away[256],status;
        int n=sscanf(line,"%u\t%u\t%255[^\t]\t%255[^\t]\t%u\t%u\t%c",
                     &round,&day,home,away,&home_score,&away_score,&status);
        CHECK(n==7 && round>=1U && round<=35U && day>=1U);
        int a=name_index(home,&registered),b=name_index(away,&registered);
        CHECK(a>=0 && b>=0 && a!=b);
        size_t ri=round-1U;CHECK(rounds[ri].fixture_count<5U);
        CHECK(rounds[ri].serie_a_matchday==0U || rounds[ri].serie_a_matchday==day);
        rounds[ri].serie_a_matchday=day;
        size_t fi=rounds[ri].fixture_count++;
        fixture_inputs[ri][fi]=(competition_fixture_input_t){uuids[a],uuids[b]};
        size_t i=ri*10U+(size_t)a,j=ri*10U+(size_t)b;
        CHECK(!seen[i] && !seen[j]);seen[i]=seen[j]=1U;
        CHECK(status=='F' || status=='P');
        fantasy_score_status_t st=status=='F'?FANTASY_SCORE_FINAL:FANTASY_SCORE_PENDING;
        scores[i]=(competition_score_input_t){uuids[a],day,{home_score,st}};
        scores[j]=(competition_score_input_t){uuids[b],day,{away_score,st}};
        ++fixture_lines;
        if (status=='F') ++final_fixture_count;
    }
    CHECK(fclose(file)==0);
    CHECK(fixture_lines==175 && final_fixture_count==40 && registered==10);
    for (size_t ri=0;ri<35U;ri++) {
        CHECK(rounds[ri].fixture_count==5U);
        CHECK(rounds[ri].serie_a_matchday==ri+4U);
        rounds[ri].fixtures=fixture_inputs[ri];
    }
    for(size_t i=0;i<350U;i++) CHECK(seen[i]);
    competition_input_t in={teams,10U,rounds,35U,scores,350U};
    competition_t *c=NULL;
    CHECK(competition_create(&in,&c)==0 && c!=NULL);
    CHECK(competition_team_count(c)==10U && competition_round_count(c)==35U);
    CHECK(!competition_is_simulation_ready(c));
    int count3=0,count4=0;uint64_t total=0;
    for(team_index_t a=0U;a<10U;a++) {
        for(team_index_t b=a+1U;b<10U;b++) {
            uint32_t k=0;CHECK(competition_pair_count(c,a,b,&k)==0);
            if(k==3U)count3++; else if(k==4U)count4++;
            else CHECK(0);
            total+=k;
        }
    }
    CHECK(count3==5 && count4==40 && total==175U);
    for(size_t ri=0;ri<35U;ri++) {
        uint32_t day;
        CHECK(competition_round_matchday(c,ri,&day)==0 && day==ri+4U);
        for(size_t ti=0;ti<10U;ti++) {
            fantasy_score_t score;
            CHECK(competition_score_at(c,(team_index_t)ti,ri,&score)==0);
            if(ri>=8U) CHECK(score.status==FANTASY_SCORE_PENDING && score.points_tenths==0U);
            else CHECK(score.status==FANTASY_SCORE_FINAL);
        }
    }
    /* Borrowed names are copies: changes to the registration input do not
     * change the successfully constructed, owned snapshot. */
    team_uuid_t id;const char *name;uint32_t sum;bool complete;
    CHECK(competition_team_at(c,0U,&id,&name,&sum,&complete)==0 && !complete);
    char original[256];(void)snprintf(original,sizeof(original),"%s",name);
    for (size_t ti=0;ti<10U;ti++) names[ti][0]='X';
    CHECK(strcmp(name,original)==0);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
int main(void) {
    if (contract()!=EXIT_SUCCESS) return EXIT_FAILURE;
    if (typical_league_sizes()!=EXIT_SUCCESS) return EXIT_FAILURE;
    if (regression()!=EXIT_SUCCESS) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
