/*
 * LeaderboardSystem versioned, stateless JSON subprocess adapter (3.1.3 PR B).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 * This executable is deliberately outside the public native library.
 */
#include "json_value.h"
#include "competition.h"
#include "calendar.h"
#include "season_resolution.h"
#include "match.h"
#include "team.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#ifndef LEADERBOARD_ENGINE_VERSION
#define LEADERBOARD_ENGINE_VERSION "3.1.2"
#endif
#ifndef LEADERBOARD_ENGINE_ID
#define LEADERBOARD_ENGINE_ID "3.1.2-json-v1"
#endif

#define INPUT_LIMIT (4U * 1024U * 1024U)
#define OUTPUT_LIMIT (8U * 1024U * 1024U)
#define MAX_TEAMS 20U
#define MAX_ROUNDS 80U
#define MAX_FIXTURES 10U
#define MAX_SCORES (MAX_TEAMS * MAX_ROUNDS)

typedef struct cli {
    const char *request_id;
    const char *error_code;
    const char *error_message;
    const char *error_path;
    int exit_code;
} cli_t;

typedef struct snapshot_storage {
    competition_team_input_t teams[MAX_TEAMS];
    competition_round_input_t rounds[MAX_ROUNDS];
    competition_fixture_input_t fixtures[MAX_ROUNDS][MAX_FIXTURES];
    competition_score_input_t scores[MAX_SCORES];
} snapshot_storage_t;

static int fail(cli_t *c, int status, const char *code, const char *msg, const char *path) {
    c->exit_code=status; c->error_code=code; c->error_message=msg; c->error_path=path;
    return -1;
}
static bool only(const json_value_t *v, const char *const *keys, size_t size) {
    return json_has_only(v,keys,size);
}
#define FIELDS(v, ...) only((v),(const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__})/sizeof(const char *))

static bool bounded_int(const json_value_t *v, uint64_t min, uint64_t max, uint64_t *out) {
    uint64_t n=0U;
    if(json_uint(v,&n) || n<min || n>max) return false;
    if(out) *out=n;
    return true;
}
static bool enum_str(const json_value_t *v, const char *a, const char *b) {
    const char *s=json_string(v);
    return s && (strcmp(s,a)==0 || (b && strcmp(s,b)==0));
}
static bool valid_uuid(const char *s) {
    team_uuid_t id;
    return s && team_uuid_parse(s,&id)==0;
}
static bool valid_engine_id(const char *s) {
    if(!s)return false;
    size_t n=strlen(s);
    if(!n || n>128U)return false;
    for(size_t i=0;i<n;++i) {
        unsigned char ch=(unsigned char)s[i];
        if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||
             (ch>='0'&&ch<='9')||ch=='.'||ch=='_'||
             ch=='+'||ch==':'||ch=='-'))return false;
    }
    return true;
}
static bool valid_request_id(const char *s) {
    if(!s) return false;
    size_t n=strlen(s);
    if(!n || n>64U) return false;
    for(size_t i=0;i<n;++i) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='A' && c<='Z')||(c>='a' && c<='z')||
             (c>='0' && c<='9')||c=='.'||c=='_'||c==':'||c=='-')) return false;
    }
    return true;
}

/* Response helpers: each failed insertion is propagated to the caller. */
static int put(json_value_t *o, const char *key, json_value_t *value) {
    if(!value) return -1;
    if(json_put(o,key,value)) {json_free(value);return -1;}
    return 0;
}
static int push(json_value_t *a,json_value_t *value) {
    if(!value) return -1;
    if(json_push(a,value)) {json_free(value);return -1;}
    return 0;
}
static int str(json_value_t *o,const char *key,const char *s) {return put(o,key,json_text(s));}
static int num(json_value_t *o,const char *key,uint64_t n) {return put(o,key,json_number(n));}
static int signed_num(json_value_t *o,const char *key,int64_t n) {return put(o,key,json_signed(n));}
static int boolean(json_value_t *o,const char *key,bool b) {return put(o,key,json_bool(b));}

static int snapshot(cli_t *c,const json_value_t *root, competition_t **result) {
    *result=NULL;
    const json_value_t *comp=json_get(root,"competition");
    if(!FIELDS(comp,"teams","rounds","scores"))
        return fail(c,2,"INVALID_REQUEST","Competition must contain only teams, rounds and scores","/competition");
    const json_value_t *teams=json_get(comp,"teams"), *rounds=json_get(comp,"rounds"),
                       *scores=json_get(comp,"scores");
    size_t n=json_count(teams), r=json_count(rounds), ns=json_count(scores);
    if(json_kind(teams)!=JSON_ARRAY || json_kind(rounds)!=JSON_ARRAY ||
       json_kind(scores)!=JSON_ARRAY)
        return fail(c,2,"INVALID_REQUEST","Competition teams, rounds and scores must be arrays","/competition");
    if(n>MAX_TEAMS || r>MAX_ROUNDS || ns>MAX_SCORES)
        return fail(c,2,"LIMIT_EXCEEDED","Competition exceeds CLI size limits","/competition");
    if(n<2U || (n%2U)!=0U || r==0U || ns<2U)
        return fail(c,3,"INVALID_SNAPSHOT","Competition needs an even team count and at least one round","/competition");
    snapshot_storage_t *storage=calloc(1U,sizeof(*storage));
    if(!storage) return fail(c,4,"RESOURCE_EXHAUSTED","Cannot allocate competition input",NULL);
    for(size_t i=0;i<n;++i) {
        const json_value_t *x=json_at(teams,i);
        const char *uuid=json_string(json_get(x,"uuid")), *name=json_string(json_get(x,"name"));
        if(!FIELDS(x,"uuid","name") || !valid_uuid(uuid) || !name ||
           strlen(name)==0U || strlen(name)>TEAM_NAME_MAX_BYTES) {
            free(storage);
            return fail(c,2,"INVALID_REQUEST","Invalid team UUID or UTF-8 name","/competition/teams");
        }
        storage->teams[i].uuid=uuid;storage->teams[i].name=name;
    }
    for(size_t i=0;i<r;++i) {
        const json_value_t *x=json_at(rounds,i), *fix=json_get(x,"fixtures");
        uint64_t day;
        if(!FIELDS(x,"serie_a_matchday","fixtures") ||
           !bounded_int(json_get(x,"serie_a_matchday"),1U,UINT32_MAX,&day) ||
           json_kind(fix)!=JSON_ARRAY || json_count(fix)!=n/2U) {
            free(storage);
            return fail(c,2,"INVALID_REQUEST","Each round needs a positive matchday and exactly N/2 fixtures","/competition/rounds");
        }
        storage->rounds[i].serie_a_matchday=(uint32_t)day;
        storage->rounds[i].fixture_count=n/2U;
        storage->rounds[i].fixtures=storage->fixtures[i];
        for(size_t j=0;j<n/2U;++j) {
            const json_value_t *f=json_at(fix,j);
            const char *home=json_string(json_get(f,"home_uuid")),
                       *away=json_string(json_get(f,"away_uuid"));
            if(!FIELDS(f,"home_uuid","away_uuid") || !valid_uuid(home) || !valid_uuid(away)) {
                free(storage);
                return fail(c,2,"INVALID_REQUEST","Fixture UUIDs must be valid","/competition/rounds");
            }
            storage->fixtures[i][j].home_uuid=home;
            storage->fixtures[i][j].away_uuid=away;
        }
    }
    for(size_t i=0;i<ns;++i) {
        const json_value_t *x=json_at(scores,i);
        const char *uuid=json_string(json_get(x,"team_uuid")),
                   *status=json_string(json_get(x,"status"));
        uint64_t day,pts;
        if(!FIELDS(x,"team_uuid","serie_a_matchday","points_tenths","status") ||
           !valid_uuid(uuid) || !enum_str(json_get(x,"status"),"FINAL","PENDING") ||
           !bounded_int(json_get(x,"serie_a_matchday"),1U,UINT32_MAX,&day) ||
           !bounded_int(json_get(x,"points_tenths"),0U,UINT32_MAX,&pts) ||
           (strcmp(status,"PENDING")==0 && pts!=0U)) {
            free(storage);
            return fail(c,2,"INVALID_REQUEST","Score requires UUID, matchday, integer tenths, and FINAL/PENDING status","/competition/scores");
        }
        storage->scores[i].team_uuid=uuid;
        storage->scores[i].serie_a_matchday=(uint32_t)day;
        storage->scores[i].score.points_tenths=(uint32_t)pts;
        storage->scores[i].score.status=strcmp(status,"FINAL")==0?FANTASY_SCORE_FINAL:FANTASY_SCORE_PENDING;
    }
    competition_input_t input={storage->teams,n,storage->rounds,r,storage->scores,ns};
    int status=competition_create(&input,result);
    free(storage);
    if(status!=0) {
        if(status==-ENOMEM) return fail(c,4,"RESOURCE_EXHAUSTED","Competition allocation failed",NULL);
        return fail(c,3,"INVALID_SNAPSHOT","Native competition validation rejected the snapshot","/competition");
    }
    return 0;
}

static size_t finalized_prefix(const competition_t *comp) {
    size_t n=competition_team_count(comp),r=competition_round_count(comp);
    for(size_t i=0;i<r;++i) {
        for(size_t j=0;j<n;++j) {
            fantasy_score_t score;
            if(competition_score_at(comp,(team_index_t)j,i,&score)!=0 ||
               score.status!=FANTASY_SCORE_FINAL) return i;
        }
    }
    return r;
}

static int parse_generator(cli_t *c, const json_value_t *x, bool effective,
                           calendar_options_t *out) {
    static const char *const keys[]={"method","sampling_mode","seed","moves_per_sample",
        "enable_three_round","three_round_attempts","three_round_node_limit",
        "chain_warmup_moves","chain_thinning_moves"};
    if(!only(x,keys,sizeof(keys)/sizeof(keys[0])))
        return fail(c,2,"INVALID_REQUEST","Generator contains unknown fields","/generator");
    const char *method=json_string(json_get(x,"method"));
    const char *mode=json_string(json_get(x,"sampling_mode"));
    uint64_t seed;
    if(!method || !mode || !enum_str(json_get(x,"method"),"shuffle","recombine") ||
       !enum_str(json_get(x,"sampling_mode"),"independent","chain") ||
       !bounded_int(json_get(x,"seed"),0U,UINT32_MAX,&seed))
        return fail(c,2,"INVALID_REQUEST","Generator needs method, sampling_mode and 32-bit seed","/generator");
    calendar_options_t g=calendar_options_default();
    g.method=strcmp(method,"shuffle")==0?CALENDAR_METHOD_ROUND_SHUFFLE:CALENDAR_METHOD_RECOMBINE;
    g.sampling_mode=strcmp(mode,"independent")==0?CALENDAR_SAMPLING_INDEPENDENT:CALENDAR_SAMPLING_CHAIN;
    g.seed=(uint32_t)seed;
    const struct {const char *name;size_t *field;uint64_t min,max;} nums[]={
        {"moves_per_sample",&g.moves_per_sample,0U,100000U},
        {"three_round_attempts",&g.three_round_attempts,1U,10000U},
        {"three_round_node_limit",&g.three_round_node_limit,1U,1000000U},
        {"chain_warmup_moves",&g.chain_warmup_moves,0U,100000U},
        {"chain_thinning_moves",&g.chain_thinning_moves,0U,100000U}
    };
    for(size_t i=0;i<sizeof(nums)/sizeof(nums[0]);++i) {
        const json_value_t *v=json_get(x,nums[i].name);
        uint64_t n;
        if(v) {
            if(!bounded_int(v,nums[i].min,nums[i].max,&n))
                return fail(c,2,"INVALID_REQUEST","Generator numeric option out of bounds","/generator");
            *nums[i].field=(size_t)n;
        } else if(effective)
            return fail(c,2,"INVALID_REQUEST","Replay generator must include every option","/calendar/generator");
    }
    const json_value_t *b=json_get(x,"enable_three_round");
    if(b) {
        if(json_boolean(b,&g.enable_three_round))
            return fail(c,2,"INVALID_REQUEST","enable_three_round must be boolean","/generator");
    } else if(effective)
        return fail(c,2,"INVALID_REQUEST","Replay generator must include every option","/calendar/generator");
    *out=g;
    return 0;
}
static int generator_json(json_value_t *out, const calendar_options_t *g) {
    return str(out,"method",g->method==CALENDAR_METHOD_ROUND_SHUFFLE?"shuffle":"recombine") ||
           str(out,"sampling_mode",g->sampling_mode==CALENDAR_SAMPLING_CHAIN?"chain":"independent") ||
           num(out,"seed",g->seed) || num(out,"moves_per_sample",g->moves_per_sample) ||
           boolean(out,"enable_three_round",g->enable_three_round) ||
           num(out,"three_round_attempts",g->three_round_attempts) ||
           num(out,"three_round_node_limit",g->three_round_node_limit) ||
           num(out,"chain_warmup_moves",g->chain_warmup_moves) ||
           num(out,"chain_thinning_moves",g->chain_thinning_moves) ? -1 : 0;
}
static json_value_t *replay_json(const calendar_options_t *g, size_t ordinal) {
    json_value_t *o=json_new(JSON_OBJECT), *opts=json_new(JSON_OBJECT);
    if(!o || !opts) {json_free(o);json_free(opts);return NULL;}
    if(str(o,"kind","generated") || generator_json(opts,g)) {
        json_free(opts);json_free(o);return NULL;
    }
    if(put(o,"generator",opts)) {json_free(o);return NULL;}
    if(num(o,"sample_index",ordinal) || str(o,"expected_engine_id",LEADERBOARD_ENGINE_ID)) {
        json_free(o);return NULL;
    }
    return o;
}
static int draw_calendar(cli_t *c,const competition_t *comp,
                         const calendar_options_t *opts,size_t count,calendar_t **out) {
    *out=NULL;
    calendar_generator_t *gen=NULL;
    int status=calendar_generator_create(comp,opts,&gen);
    if(status) {
        if(status==-ENOTSUP)return fail(c,3,"UNSUPPORTED_CALENDAR","Original fixtures cannot satisfy generator cooldown constraints","/competition/rounds");
        if(status==-ENOMEM)return fail(c,4,"RESOURCE_EXHAUSTED","Generator allocation failed",NULL);
        return fail(c,3,"INVALID_SNAPSHOT","Generator rejected the competition or options","/generator");
    }
    for(size_t i=0;i<count;++i) {
        calendar_t *next=NULL;
        status=calendar_generator_next(gen,&next,NULL);
        if(status)break;
        calendar_destroy(*out);*out=next;
    }
    calendar_generator_destroy(gen);
    if(status) {
        calendar_destroy(*out);*out=NULL;
        return fail(c,status==-ENOMEM?4:3,status==-ENOMEM?"RESOURCE_EXHAUSTED":"UNSUPPORTED_CALENDAR",
                    "Calendar generation did not complete",NULL);
    }
    status=calendar_validate(comp,*out);
    if(status!=0) {
        calendar_destroy(*out);*out=NULL;
        return fail(c,status==-ENOMEM?4:1,status==-ENOMEM?"RESOURCE_EXHAUSTED":"INTERNAL_ERROR",
                    "Generated calendar failed native validation",NULL);
    }
    return 0;
}
static int uuid_for(const competition_t *c, team_index_t idx, char output[TEAM_UUID_TEXT_SIZE]) {
    team_uuid_t id;const char *name=NULL;uint32_t total=0;bool complete=false;
    if(competition_team_at(c,idx,&id,&name,&total,&complete))return -1;
    return team_uuid_format(&id,output);
}
static int fixture_json(json_value_t *array, const competition_t *comp, fixture_t f) {
    char home[TEAM_UUID_TEXT_SIZE],away[TEAM_UUID_TEXT_SIZE];
    if(uuid_for(comp,f.home,home) || uuid_for(comp,f.away,away))return -1;
    json_value_t *obj=json_new(JSON_OBJECT);
    if(!obj)return -1;
    if(str(obj,"home_uuid",home)||str(obj,"away_uuid",away)) {
        json_free(obj);return -1;
    }
    if(push(array,obj))return -1;
    return 0;
}
static int generate_response(cli_t *c,const json_value_t *req, const competition_t *comp,json_value_t *result) {
    calendar_options_t opts;
    if(parse_generator(c,json_get(req,"generator"),false,&opts))return -1;
    uint64_t ordinal;
    if(!bounded_int(json_get(req,"sample_index"),1U,10000U,&ordinal))
        return fail(c,2,"INVALID_REQUEST","sample_index must be in 1..10000","/sample_index");
    calendar_t *calendar=NULL;
    if(draw_calendar(c,comp,&opts,(size_t)ordinal,&calendar))return -1;
    json_value_t *cal=json_new(JSON_OBJECT),*rounds=json_new(JSON_ARRAY);
    if(!cal || !rounds) goto oom;
    if(str(cal,"kind","generated"))goto oom;
    for(size_t i=0;i<calendar_round_count(calendar);++i) {
        uint32_t day;
        if(calendar_round_matchday(calendar,i,&day))goto oom;
        json_value_t *r=json_new(JSON_OBJECT),*fixtures=json_new(JSON_ARRAY);
        if(!r || !fixtures) {json_free(r);json_free(fixtures);goto oom;}
        if(num(r,"serie_a_matchday",day)) {json_free(r);json_free(fixtures);goto oom;}
        size_t take=calendar_team_count(calendar)/2U;
        fixture_t ordered[MAX_FIXTURES];
        char home[MAX_FIXTURES][TEAM_UUID_TEXT_SIZE],away[MAX_FIXTURES][TEAM_UUID_TEXT_SIZE];
        for(size_t j=0;j<take;++j) {
            if(calendar_fixture_at(calendar,i,j,&ordered[j]) ||
               uuid_for(comp,ordered[j].home,home[j]) ||
               uuid_for(comp,ordered[j].away,away[j])) {
                json_free(r);json_free(fixtures);goto oom;
            }
        }
        size_t indices[MAX_FIXTURES];
        for(size_t j=0;j<take;++j)indices[j]=j;
        for(size_t j=1;j<take;++j) {
            size_t at=j;
            while(at>0U && (strcmp(home[indices[at-1U]],home[indices[at]])>0 ||
               (strcmp(home[indices[at-1U]],home[indices[at]])==0 &&
                strcmp(away[indices[at-1U]],away[indices[at]])>0))) {
                size_t sw=indices[at-1U];indices[at-1U]=indices[at];indices[at]=sw;--at;
            }
        }
        for(size_t j=0;j<take;++j)
            if(fixture_json(fixtures,comp,ordered[indices[j]])) {
                json_free(r);json_free(fixtures);goto oom;
            }
        if(put(r,"fixtures",fixtures)) {json_free(r);goto oom;}
        if(push(rounds,r))goto oom;
    }
    if(put(cal,"rounds",rounds)) {rounds=NULL;goto oom;}
    rounds=NULL;
    char fingerprint[17];snprintf(fingerprint,sizeof(fingerprint),"%016" PRIx64,calendar_fingerprint(calendar));
    if(str(cal,"fingerprint",fingerprint))goto oom;
    if(put(result,"calendar",cal)) {cal=NULL;goto oom;}
    cal=NULL;
    if(put(result,"replay",replay_json(&opts,(size_t)ordinal)))goto oom;
    calendar_destroy(calendar);
    return 0;
oom:json_free(cal);json_free(rounds);calendar_destroy(calendar);
    return fail(c,4,"RESOURCE_EXHAUSTED","Cannot construct calendar response",NULL);
}

static int parse_rules(cli_t *c,const json_value_t *x,match_rules_t *rules) {
    if(match_rules_default(rules))return fail(c,1,"INTERNAL_ERROR","Cannot initialize match rules",NULL);
    if(!x)return 0;
    static const char *const names[]={"goal_thresholds_tenths","post_eighth_step_tenths",
        "limit_victory_enabled","limit_victory_max_gap_tenths","limit_victory_include_subthreshold",
        "limit_draw_enabled","limit_draw_min_gap_tenths","limit_draw_include_subthreshold",
        "home_advantage_enabled","home_advantage_tenths","home_advantage_exclude_final_cycle"};
    if(!only(x,names,sizeof(names)/sizeof(names[0])))
        return fail(c,2,"INVALID_REQUEST","Unknown match rule","/rules");
    const json_value_t *thresholds=json_get(x,names[0]);
    if(json_kind(thresholds)!=JSON_ARRAY || json_count(thresholds)!=8U)
        return fail(c,2,"INVALID_REQUEST","All eight goal thresholds are required","/rules/goal_thresholds_tenths");
    for(size_t i=0;i<8U;++i) {
        uint64_t n;
        if(!bounded_int(json_at(thresholds,i),0U,UINT32_MAX,&n))
            return fail(c,2,"INVALID_REQUEST","Goal threshold must be an integer","/rules/goal_thresholds_tenths");
        rules->goal_thresholds_tenths[i]=(uint32_t)n;
    }
    const struct {const char *name;uint32_t *dst;} ints[]={
        {names[1],&rules->post_eighth_step_tenths},{names[3],&rules->limit_victory_max_gap_tenths},
        {names[6],&rules->limit_draw_min_gap_tenths},{names[9],&rules->home_advantage_tenths}
    };
    for(size_t i=0;i<sizeof(ints)/sizeof(ints[0]);++i) {
        uint64_t n;
        if(!bounded_int(json_get(x,ints[i].name),0U,UINT32_MAX,&n))
            return fail(c,2,"INVALID_REQUEST","All rule numbers must be unsigned integers","/rules");
        *ints[i].dst=(uint32_t)n;
    }
    const struct {const char *name;bool *dst;} flags[]={
        {names[2],&rules->limit_victory_enabled},{names[4],&rules->limit_victory_include_subthreshold},
        {names[5],&rules->limit_draw_enabled},{names[7],&rules->limit_draw_include_subthreshold},
        {names[8],&rules->home_advantage_enabled},{names[10],&rules->home_advantage_exclude_final_cycle}
    };
    for(size_t i=0;i<sizeof(flags)/sizeof(flags[0]);++i)
        if(json_boolean(json_get(x,flags[i].name),flags[i].dst))
            return fail(c,2,"INVALID_REQUEST","All rule flags must be booleans","/rules");
    if(match_rules_validate(rules))return fail(c,2,"INVALID_REQUEST","Native scoring-rule validation failed","/rules");
    return 0;
}
static int parse_scope(cli_t *c, const json_value_t *req, season_resolution_scope_t *scope) {
    const char *name=json_string(json_get(req,"scope"));
    if(!name || !enum_str(json_get(req,"scope"),"FULL_SEASON","FINALIZED_PREFIX"))
        return fail(c,2,"INVALID_REQUEST","Unknown season resolution scope","/scope");
    *scope=strcmp(name,"FULL_SEASON")==0?SEASON_RESOLVE_FULL_SEASON:SEASON_RESOLVE_FINALIZED_PREFIX;
    return 0;
}
static int parse_backend(cli_t *c,const json_value_t *req,season_ranking_backend_t *backend) {
    const json_value_t *v=json_get(req,"ranking_backend");
    if(!v) {*backend=SEASON_RANKING_HEAPSORT;return 0;}
    if(!enum_str(v,"heapsort","avl"))return fail(c,2,"INVALID_REQUEST","Unknown ranking backend","/ranking_backend");
    *backend=strcmp(json_string(v),"avl")==0?SEASON_RANKING_AVL:SEASON_RANKING_HEAPSORT;
    return 0;
}

/* Return a fully owned JSON object for a native sorted ranking entry. */
static json_value_t *rank_json(const season_resolution_t *r, size_t position) {
    season_rank_entry_t e;
    if(season_resolution_rank_at(r,position,&e))return NULL;
    team_uuid_t id;
    if(season_resolution_team_uuid_at(r,e.team_index,&id))return NULL;
    char uuid[TEAM_UUID_TEXT_SIZE];
    if(team_uuid_format(&id,uuid))return NULL;
    json_value_t *o=json_new(JSON_OBJECT);
    if(!o)return NULL;
    uint64_t played=(uint64_t)e.standings.wins+e.standings.draws+e.standings.losses;
    if(str(o,"team_uuid",uuid)||num(o,"sporting_rank",e.sporting_rank)||
       num(o,"championship_points",e.standings.championship_points)||
       num(o,"fantasy_total_tenths",e.fantasy_total_tenths)||
       signed_num(o,"goal_difference",e.goal_difference)||num(o,"played",played)||
       num(o,"wins",e.standings.wins)||num(o,"draws",e.standings.draws)||
       num(o,"losses",e.standings.losses)||num(o,"goals_for",e.goal_stats.goals_for)||
       num(o,"goals_against",e.goal_stats.goals_against)) {json_free(o);return NULL;}
    return o;
}
static json_value_t *resolution_json(cli_t *c,const season_resolution_t *r,
                                     const competition_t *comp,const char *kind,
                                     const char *scope,size_t total_rounds) {
    (void)c;
    json_value_t *o=json_new(JSON_OBJECT), *ranks=json_new(JSON_ARRAY), *fixtures=json_new(JSON_ARRAY);
    if(!o || !ranks || !fixtures)goto fail;
    size_t count=season_resolution_round_count(r);
    if(str(o,"scope",scope)||str(o,"calendar_kind",kind)||num(o,"evaluated_rounds",count)||
       boolean(o,"provisional",count!=total_rounds))goto fail;
    for(size_t i=0;i<season_resolution_team_count(r);++i)
        if(push(ranks,rank_json(r,i)))goto fail;
    if(put(o,"standings",ranks)) {ranks=NULL;goto fail;}
    ranks=NULL;
    /* Stable fixture order is by round, then by the home/away UUIDs. */
    for(size_t ri=0;ri<count;++ri) {
        size_t start=ri*(season_resolution_team_count(r)/2U);
        size_t take=season_resolution_team_count(r)/2U;
        season_fixture_result_t records[MAX_FIXTURES];
        char home[MAX_FIXTURES][TEAM_UUID_TEXT_SIZE],away[MAX_FIXTURES][TEAM_UUID_TEXT_SIZE];
        for(size_t j=0;j<take;++j) {
            if(season_resolution_fixture_at(r,start+j,&records[j]) ||
               uuid_for(comp,records[j].fixture.home,home[j]) ||
               uuid_for(comp,records[j].fixture.away,away[j]))goto fail;
        }
        size_t idx[MAX_FIXTURES];
        for(size_t j=0;j<take;++j)idx[j]=j;
        for(size_t j=1;j<take;++j) {
            size_t at=j;
            while(at>0U && (strcmp(home[idx[at-1U]],home[idx[at]])>0 ||
               (strcmp(home[idx[at-1U]],home[idx[at]])==0 && strcmp(away[idx[at-1U]],away[idx[at]])>0))) {
                size_t sw=idx[at-1U];idx[at-1U]=idx[at];idx[at]=sw;--at;
            }
        }
        for(size_t j=0;j<take;++j) {
            size_t k=idx[j];
            const match_result_t *m=&records[k].result;
            const char *adjust=m->adjustment==MATCH_ADJUST_NONE?"none":
                m->adjustment==MATCH_ADJUST_LIMIT_VICTORY?"limit_victory":"limit_draw";
            json_value_t *item=json_new(JSON_OBJECT);
            if(!item)goto fail;
            if(num(item,"round_index",ri)||num(item,"serie_a_matchday",records[k].serie_a_matchday)||
               str(item,"home_uuid",home[k])||str(item,"away_uuid",away[k])||
               num(item,"home_fantasy_tenths",m->home_fantasy_tenths)||
               num(item,"away_fantasy_tenths",m->away_fantasy_tenths)||
               num(item,"home_goals",m->home_goals)||num(item,"away_goals",m->away_goals)||
               str(item,"adjustment",adjust)) {json_free(item);goto fail;}
            if(push(fixtures,item))goto fail;
        }
    }
    if(put(o,"fixtures",fixtures)) {fixtures=NULL;goto fail;}
    return o;
fail:json_free(ranks);json_free(fixtures);json_free(o);return NULL;
}
static int resolve_response(cli_t *c,const json_value_t *req, const competition_t *comp,json_value_t **result) {
    season_resolution_scope_t scope;
    season_ranking_backend_t backend;
    match_rules_t rules;
    if(parse_scope(c,req,&scope)||parse_backend(c,req,&backend)||
       parse_rules(c,json_get(req,"rules"),&rules))return -1;
    const json_value_t *sel=json_get(req,"calendar");
    const char *kind=json_string(json_get(sel,"kind"));
    if(!kind || (strcmp(kind,"original")!=0 && strcmp(kind,"generated")!=0))
        return fail(c,2,"INVALID_REQUEST","Calendar must select original or generated","/calendar");
    if(strcmp(kind,"original")==0) {
        if(!FIELDS(sel,"kind"))return fail(c,2,"INVALID_REQUEST","Original calendar selector has unknown fields","/calendar");
    } else if(!FIELDS(sel,"kind","generator","sample_index","expected_engine_id"))
        return fail(c,2,"INVALID_REQUEST","Generated selector has unknown fields","/calendar");
    calendar_t *calendar=NULL;
    if(strcmp(kind,"generated")==0) {
        const char *expected=json_string(json_get(sel,"expected_engine_id"));
        if(!valid_engine_id(expected))
            return fail(c,2,"INVALID_REQUEST","Generated replay requires a valid engine ID","/calendar/expected_engine_id");
        if(strcmp(expected,LEADERBOARD_ENGINE_ID)!=0)
            return fail(c,3,"REPLAY_MISMATCH","Replay requires the original engine build","/calendar/expected_engine_id");
        calendar_options_t opts;
        uint64_t ordinal;
        if(parse_generator(c,json_get(sel,"generator"),true,&opts))return -1;
        if(!bounded_int(json_get(sel,"sample_index"),1U,10000U,&ordinal))
            return fail(c,2,"INVALID_REQUEST","Replay sample_index out of bounds","/calendar/sample_index");
        if(draw_calendar(c,comp,&opts,(size_t)ordinal,&calendar))return -1;
    }
    season_resolution_options_t opts={scope,backend};
    season_resolution_t *resolution=NULL;
    int rc=season_resolve_with_options(comp,calendar,&rules,&opts,&resolution);
    calendar_destroy(calendar);
    if(rc) {
        if(rc==-EAGAIN)return fail(c,3,"PENDING_SCORES","FULL_SEASON requires FINAL scores for all teams and rounds","/scope");
        if(rc==-ENOMEM)return fail(c,4,"RESOURCE_EXHAUSTED","Season resolution allocation failed",NULL);
        return fail(c,3,"INVALID_SNAPSHOT","Native season resolution rejected the request",NULL);
    }
    json_value_t *native=resolution_json(c,resolution,comp,kind,
                         scope==SEASON_RESOLVE_FULL_SEASON?"FULL_SEASON":"FINALIZED_PREFIX",
                         competition_round_count(comp));
    season_resolution_destroy(resolution);
    if(!native)return fail(c,4,"RESOURCE_EXHAUSTED","Cannot construct resolution response",NULL);
    *result=native;
    return 0;
}
static json_value_t *validate_response(const competition_t *comp) {
    json_value_t *o=json_new(JSON_OBJECT);
    if(!o)return NULL;
    if(num(o,"team_count",competition_team_count(comp)) ||
       num(o,"round_count",competition_round_count(comp)) ||
       boolean(o,"simulation_ready",competition_is_simulation_ready(comp)) ||
       num(o,"finalized_prefix_rounds",finalized_prefix(comp))) {
        json_free(o);return NULL;
    }
    return o;
}

static json_value_t *envelope(const cli_t *c, const char *operation,
                               json_value_t *result) {
    json_value_t *o=json_new(JSON_OBJECT), *engine=json_new(JSON_OBJECT);
    if(!o || !engine) {
        json_free(o);json_free(engine);json_free(result);return NULL;
    }
    if(str(engine,"version",LEADERBOARD_ENGINE_VERSION) ||
       str(engine,"id",LEADERBOARD_ENGINE_ID)) {
        json_free(o);json_free(engine);json_free(result);return NULL;
    }
    if(str(o,"schema_version","1.0") ||
       put(o,"request_id",c->request_id?json_text(c->request_id):json_new(JSON_NULL))) {
        json_free(o);json_free(engine);json_free(result);return NULL;
    }
    if(put(o,"engine",engine)) {json_free(o);json_free(result);return NULL;}
    if(boolean(o,"ok",c->error_code==NULL)) {json_free(o);json_free(result);return NULL;}
    if(c->error_code) {
        json_value_t *error=json_new(JSON_OBJECT);
        if(!error) {json_free(o);json_free(result);return NULL;}
        if(str(error,"code",c->error_code) || str(error,"message",c->error_message) ||
           put(error,"path",c->error_path?json_text(c->error_path):json_new(JSON_NULL))) {
            json_free(error);json_free(o);json_free(result);return NULL;
        }
        if(put(o,"error",error)) {json_free(o);json_free(result);return NULL;}
        json_free(result);
    } else {
        if(str(o,"operation",operation)) {json_free(o);json_free(result);return NULL;}
        if(put(o,"result",result)) {json_free(o);return NULL;}
    }
    return o;
}

static int process(cli_t *c, const json_value_t *req, const char **operation,
                   json_value_t **result) {
    *result=NULL;
    if(json_kind(req)!=JSON_OBJECT)
        return fail(c,2,"INVALID_REQUEST","Request must be an object",NULL);
    /* Echo only verified request identifiers, never untrusted arbitrary data. */
    const char *id=json_string(json_get(req,"request_id"));
    if(valid_request_id(id))c->request_id=id;
    const json_value_t *v=json_get(req,"schema_version");
    if(!v || json_kind(v)!=JSON_STRING || strcmp(json_string(v),"1.0")!=0)
        return fail(c,2,"INVALID_SCHEMA_VERSION","Supported JSON CLI schema_version is 1.0","/schema_version");
    if(!c->request_id)
        return fail(c,2,"INVALID_REQUEST","request_id must be 1..64 ASCII identifier characters","/request_id");
    *operation=json_string(json_get(req,"operation"));
    if(!*operation)return fail(c,2,"INVALID_REQUEST","operation must be a string","/operation");
    const char *op=*operation;
    if(strcmp(op,"simulate")==0)
        return fail(c,2,"INVALID_REQUEST","simulate is reserved for milestone 3.1.3 PR C","/operation");
    if(strcmp(op,"validate")!=0 && strcmp(op,"generate")!=0 && strcmp(op,"resolve")!=0)
        return fail(c,2,"INVALID_REQUEST","Unknown operation","/operation");
    if((strcmp(op,"validate")==0 && !FIELDS(req,"schema_version","request_id","operation","competition")) ||
       (strcmp(op,"generate")==0 && !FIELDS(req,"schema_version","request_id","operation","competition","generator","sample_index")) ||
       (strcmp(op,"resolve")==0 && !FIELDS(req,"schema_version","request_id","operation","competition","scope","calendar","rules","ranking_backend")))
        return fail(c,2,"INVALID_REQUEST","Request contains unsupported or unknown properties",NULL);
    competition_t *comp=NULL;
    if(snapshot(c,req,&comp))return -1;
    int status=0;
    if(strcmp(op,"validate")==0) {
        *result=validate_response(comp);
        if(!*result)status=fail(c,4,"RESOURCE_EXHAUSTED","Cannot construct validation response",NULL);
    } else if(strcmp(op,"generate")==0) {
        *result=json_new(JSON_OBJECT);
        if(!*result)status=fail(c,4,"RESOURCE_EXHAUSTED","Cannot allocate calendar response",NULL);
        else status=generate_response(c,req,comp,*result);
    } else status=resolve_response(c,req,comp,result);
    competition_destroy(comp);
    if(status) {json_free(*result);*result=NULL;}
    return status;
}

static int write_final(const cli_t *c,const char *op,json_value_t *result) {
    json_value_t *out=envelope(c,op,result);
    if(!out)return 1;
    char *line=json_serialize(out,OUTPUT_LIMIT-1U);
    json_free(out);
    if(!line) {
        /* Only a small bounded error is emitted on oversized successful output. */
        cli_t fallback=*c;
        fallback.error_code="LIMIT_EXCEEDED";
        fallback.error_message="Serialized response exceeds the 8 MiB limit";
        fallback.error_path=NULL;
        fallback.exit_code=2;
        json_value_t *error=envelope(&fallback,NULL,NULL);
        if(!error)return 1;
        line=json_serialize(error,OUTPUT_LIMIT-1U);
        json_free(error);
        if(!line)return 1;
        if(puts(line)<0 || fflush(stdout)!=0) {free(line);return 5;}
        free(line);return 2;
    }
    if(puts(line)<0 || fflush(stdout)!=0) {free(line);return 5;}
    free(line);
    return c->error_code?c->exit_code:0;
}
int main(int argc, char **argv) {
    (void)argv;
    cli_t c={0};
#if defined(_WIN32)
    /* Windows CRT text mode otherwise rewrites JSON LF bytes to CRLF. */
    if(_setmode(_fileno(stdin),_O_BINARY)==-1 ||
       _setmode(_fileno(stdout),_O_BINARY)==-1 ||
       _setmode(_fileno(stderr),_O_BINARY)==-1) {
        fail(&c,5,"IO_ERROR","Cannot configure binary JSON streams",NULL);
        return write_final(&c,NULL,NULL);
    }
#endif
    if(argc!=1) {
        fail(&c,2,"INVALID_REQUEST","CLI v1 accepts JSON on stdin without command-line arguments",NULL);
        return write_final(&c,NULL,NULL);
    }
    char *buf=malloc(INPUT_LIMIT+1U);
    if(!buf) {
        fail(&c,4,"RESOURCE_EXHAUSTED","Cannot allocate input buffer",NULL);
        return write_final(&c,NULL,NULL);
    }
    size_t size=0U;int ch;
    while(size<=INPUT_LIMIT && (ch=fgetc(stdin))!=EOF)buf[size++]=(char)ch;
    if(ferror(stdin)) {
        fail(&c,5,"IO_ERROR","Cannot read stdin",NULL);
        free(buf);return write_final(&c,NULL,NULL);
    }
    if(size>INPUT_LIMIT) {
        fail(&c,2,"LIMIT_EXCEEDED","JSON stdin request exceeds 4 MiB",NULL);
        free(buf);return write_final(&c,NULL,NULL);
    }
    buf[size]='\0';
    json_value_t *req=NULL;
    int parsed=json_parse(buf,size,&req);
    free(buf);
    if(parsed) {
        fail(&c,parsed==-2?4:2,parsed==-2?"RESOURCE_EXHAUSTED":"INVALID_JSON",
             parsed==-2?"Cannot allocate JSON parse tree":"Malformed JSON, invalid UTF-8 or duplicate object keys",NULL);
        return write_final(&c,NULL,NULL);
    }
    const char *operation=NULL;
    json_value_t *result=NULL;
    (void)process(&c,req,&operation,&result);
    /* The response borrows request_id, so keep the request tree until written. */
    int outcome=write_final(&c,operation,result);
    json_free(req);
    return outcome;
}
