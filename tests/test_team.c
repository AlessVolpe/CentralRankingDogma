/* LeaderboardSystem 3.0.11: UUID and deep-owned team model. */
#include "team.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return EXIT_FAILURE; } } while (0)
int main(void) {
    const char *uuid="A0B10000-1111-4222-8333-112233445566";
    team_uuid_t one, two;
    char text[TEAM_UUID_TEXT_SIZE];
    CHECK(team_uuid_parse(uuid,&one)==0);
    CHECK(team_uuid_format(&one,text)==0);
    CHECK(strcmp(text,"a0b10000-1111-4222-8333-112233445566")==0);
    CHECK(team_uuid_parse(text,&two)==0 && team_uuid_compare(&one,&two)==0);
    CHECK(team_uuid_parse(NULL,&one)!=0);
    CHECK(team_uuid_parse("",&one)!=0);
    CHECK(team_uuid_parse("a0b10000-1111-4222-8333-11223344556z",&one)!=0);
    CHECK(team_uuid_parse("a0b10000-1111-4222-8333-112233445566extra",&one)!=0);
    CHECK(team_uuid_parse("a0b100001111-4222-8333-112233445566",&one)!=0);
    CHECK(team_uuid_parse("a0b10000-1111-4222-8333-112233445566",NULL)!=0);
    CHECK(team_uuid_format(NULL,text)!=0);
    CHECK(team_uuid_format(&one,NULL)!=0);
    CHECK(team_create(uuid,NULL,0U)==NULL);
    CHECK(team_create(uuid,"",0U)==NULL);
    CHECK(team_create("garbage","Boh",0U)==NULL);
    char long_name[TEAM_NAME_MAX_BYTES + 2U];
    memset(long_name,'a', sizeof(long_name)-1U);long_name[sizeof(long_name)-1U]='\0';
    CHECK(team_create(uuid,long_name,0U)==NULL);
    char name[]="Brigate Ebosse";
    team_t *team=team_create(uuid,name,24835U);
    CHECK(team!=NULL);
    CHECK(team->name!=name);
    CHECK(strcmp(team->name,name)==0 && team->total_points_tenths==24835U);
    CHECK(team_uuid_compare(&team->id,&two)==0);
    name[0]='b';
    CHECK(strcmp(team->name,"Brigate Ebosse")==0);
    team_destroy(team);
    team_destroy(NULL);
    return EXIT_SUCCESS;
}
