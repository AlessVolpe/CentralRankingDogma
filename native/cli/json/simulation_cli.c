/*
 * LeaderboardSystem private JSON Monte Carlo adapter (3.1.3 PR C).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "simulation_cli.h"
#include "team.h"
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_MAX_TEAMS 20U
#define SIM_MAX_SAMPLES 10000U
#define SIM_MAX_ATTEMPTS 100000U
#define SIM_UNIQUENESS_MAX (256U * 1024U * 1024U)
#define SIM_UNIQUENESS_DEFAULT (64U * 1024U * 1024U)

static volatile sig_atomic_t stop_requested = 0;

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static int set_error(simulation_error_t *e, int exit_code,
                     const char *code, const char *message, const char *path) {
    e->exit_code = exit_code;
    e->code = code;
    e->message = message;
    e->path = path;
    return -1;
}

static int insert(json_value_t *obj, const char *key, json_value_t *value) {
    if (!value) return -1;
    if (json_put(obj, key, value)) { json_free(value); return -1; }
    return 0;
}
static int append(json_value_t *array, json_value_t *value) {
    if (!value) return -1;
    if (json_push(array, value)) { json_free(value); return -1; }
    return 0;
}
static int text(json_value_t *obj, const char *key, const char *value) {
    return insert(obj, key, json_text(value));
}
static int integer(json_value_t *obj, const char *key, uint64_t value) {
    return insert(obj, key, json_number(value));
}
static int signed_integer(json_value_t *obj, const char *key, int64_t value) {
    return insert(obj, key, json_signed(value));
}
static int boolean(json_value_t *obj, const char *key, bool value) {
    return insert(obj, key, json_bool(value));
}

/* JSON is written in the C numeric locale (the default process locale).
 * Create a numeric node from a bounded decimal representation, never from a
 * string node, so JSON Schema sees a number and zero remains unambiguous. */
static json_value_t *json_finite_double(double value) {
    if (!isfinite(value)) return NULL;
    char buffer[64];
    int length = snprintf(buffer, sizeof(buffer), "%.17g", value);
    if (length <= 0 || (size_t)length >= sizeof(buffer)) return NULL;
    json_value_t *node = NULL;
    if (json_parse(buffer, (size_t)length, &node)) return NULL;
    return node;
}
static int nullable_number(json_value_t *obj, const char *key,
                           bool present, double value) {
    return insert(obj, key, present ? json_finite_double(value) : json_new(JSON_NULL));
}
static int nullable_size(json_value_t *obj, const char *key,
                         bool present, size_t value) {
    return insert(obj, key, present ? json_number(value) : json_new(JSON_NULL));
}

static int uint_between(const json_value_t *obj, const char *key,
                        uint64_t min, uint64_t max, uint64_t *out) {
    uint64_t n = 0U;
    if (json_uint(json_get(obj,key), &n) || n < min || n > max) return -1;
    *out = n;
    return 0;
}
static int opt_bool(const json_value_t *obj, const char *key, bool *out) {
    const json_value_t *value = json_get(obj,key);
    return value ? json_boolean(value,out) : 0;
}
static int opt_size(const json_value_t *obj, const char *key,
                    uint64_t min, uint64_t max, size_t *out) {
    if (!json_get(obj,key)) return 0;
    uint64_t number = 0;
    if (uint_between(obj,key,min,max,&number)) return -1;
    *out = (size_t)number;
    return 0;
}

typedef struct progress_context {
    const char *request_id; /* validated ASCII identifier from main.c */
    bool emit;
    bool io_failed;
    size_t last_attempts;
    size_t last_accepted;
} progress_context_t;

static bool emit_progress(progress_context_t *p, size_t attempts, size_t accepted) {
    if (!p->emit ||
        (p->last_attempts == attempts && p->last_accepted == accepted))
        return true;
    if (fprintf(stderr,
        "{\"schema_version\":\"1.0\",\"request_id\":\"%s\","
        "\"event\":\"progress\",\"attempts\":%zu,\"accepted_samples\":%zu}\n",
        p->request_id, attempts, accepted) < 0 || fflush(stderr) != 0) {
        p->io_failed = true;
        return false;
    }
    p->last_attempts = attempts;
    p->last_accepted = accepted;
    return true;
}
static bool progress_callback(const monte_carlo_progress_t *progress, void *opaque) {
    progress_context_t *p = opaque;
    if (!emit_progress(p,progress->generation_attempts,progress->accepted_samples))
        return false;
    return stop_requested == 0;
}

/* C11 exclusive-create mode prevents two concurrent invocations from sharing
 * a partial file. The target is trusted by the Python worker, not parsed from
 * attacker-supplied JSON. No final file is opened until the batch succeeds. */
static FILE *stage_csv(simulation_csv_t *stage, const char *destination) {
    if (!destination || !*destination) return NULL;
    size_t length = strlen(destination);
    if (length > 4096U || length > SIZE_MAX - 64U) return NULL;
    stage->destination_path = malloc(length+1U);
    stage->staging_path = malloc(length+64U);
    if (!stage->destination_path || !stage->staging_path) return NULL;
    memcpy(stage->destination_path,destination,length+1U);
    for (unsigned attempt = 0U; attempt < 1000U; ++attempt) {
        int n = snprintf(stage->staging_path,length+64U,
                         "%s.lb-partial-%03u",destination,attempt);
        if (n < 0 || (size_t)n >= length+64U) return NULL;
        FILE *stream = fopen(stage->staging_path,"wbx");
        if (stream) { stage->created = true; return stream; }
        if (errno != EEXIST) return NULL;
    }
    return NULL;
}
void simulation_csv_discard(simulation_csv_t *csv) {
    if (!csv) return;
    if (csv->created && csv->staging_path) (void)remove(csv->staging_path);
    free(csv->staging_path);
    free(csv->destination_path);
    csv->staging_path = NULL;
    csv->destination_path = NULL;
    csv->created = false;
}
int simulation_csv_commit(simulation_csv_t *csv) {
    if (!csv || !csv->staging_path || !csv->destination_path) return -1;
    if (rename(csv->staging_path,csv->destination_path) != 0) return -1;
    free(csv->staging_path);
    free(csv->destination_path);
    csv->staging_path = NULL;
    csv->destination_path = NULL;
    csv->created = false;
    return 0;
}

static json_value_t *baseline_json(const season_resolution_t *baseline) {
    json_value_t *array = json_new(JSON_ARRAY);
    if (!array) return NULL;
    size_t teams = season_resolution_team_count(baseline);
    for (size_t i = 0U; i < teams; ++i) {
        season_rank_entry_t rank;
        team_uuid_t id;
        char uuid[TEAM_UUID_TEXT_SIZE];
        if (season_resolution_rank_at(baseline,i,&rank) ||
            season_resolution_team_uuid_at(baseline,rank.team_index,&id) ||
            team_uuid_format(&id,uuid)) goto fail;
        json_value_t *entry = json_new(JSON_OBJECT);
        if (!entry) goto fail;
        uint64_t played = (uint64_t)rank.standings.wins +
                          rank.standings.draws + rank.standings.losses;
        if (text(entry,"team_uuid",uuid) ||
            integer(entry,"sporting_rank",rank.sporting_rank) ||
            integer(entry,"championship_points",rank.standings.championship_points) ||
            integer(entry,"fantasy_total_tenths",rank.fantasy_total_tenths) ||
            signed_integer(entry,"goal_difference",rank.goal_difference) ||
            integer(entry,"played",played) ||
            integer(entry,"wins",rank.standings.wins) ||
            integer(entry,"draws",rank.standings.draws) ||
            integer(entry,"losses",rank.standings.losses) ||
            integer(entry,"goals_for",rank.goal_stats.goals_for) ||
            integer(entry,"goals_against",rank.goal_stats.goals_against)) {
            json_free(entry);
            goto fail;
        }
        if (append(array,entry)) goto fail;
    }
    return array;
fail:
    json_free(array);
    return NULL;
}

static json_value_t *basic_json(const monte_carlo_result_t *result,
                                const uint32_t baseline_ranks[SIM_MAX_TEAMS]) {
    json_value_t *out = json_new(JSON_OBJECT), *teams = json_new(JSON_ARRAY);
    if (!out || !teams) goto fail;
    size_t count = monte_carlo_result_team_count(result);
    size_t accepted = monte_carlo_result_accepted(result);
    for (size_t i = 0U; i < count; ++i) {
        monte_carlo_team_stats_t stats;
        char uuid[TEAM_UUID_TEXT_SIZE];
        if (monte_carlo_result_team_at(result,(team_index_t)i,&stats) ||
            team_uuid_format(&stats.uuid,uuid)) goto fail;
        json_value_t *team = json_new(JSON_OBJECT);
        if (!team) goto fail;
        double weighted_sum = 0.0;
        for (uint32_t rank = 1U; rank <= count; ++rank) {
            size_t observations = 0;
            if (monte_carlo_result_rank_frequency(result,(team_index_t)i,
                                                  rank,&observations)) {
                json_free(team); goto fail;
            }
            weighted_sum += (double)rank * (double)observations;
        }
        if (text(team,"team_uuid",uuid) ||
            integer(team,"baseline_rank",baseline_ranks[i]) ||
            nullable_number(team,"first_place_frequency",accepted!=0U,
                 accepted ? (double)stats.first_place_finishes/(double)accepted : 0.0) ||
            nullable_number(team,"mean_sporting_rank",accepted!=0U,
                 accepted ? weighted_sum/(double)accepted : 0.0) ||
            nullable_number(team,"mean_championship_points",stats.has_observations,
                 stats.mean_points)) {
            json_free(team); goto fail;
        }
        if (append(teams,team)) goto fail;
    }
    if (insert(out,"teams",teams)) {teams=NULL;goto fail;}
    return out;
fail:
    json_free(teams);
    json_free(out);
    return NULL;
}

static json_value_t *diagnostics_json(const calendar_diagnostics_t *d) {
    json_value_t *out = json_new(JSON_OBJECT);
    if (!out) return NULL;
    if (integer(out,"two_round_attempts",d->two_round_attempts) ||
        integer(out,"two_round_partial_moves",d->two_round_partial_moves) ||
        integer(out,"two_round_whole_swaps",d->two_round_whole_swaps) ||
        integer(out,"three_round_attempts",d->three_round_attempts) ||
        integer(out,"three_round_moves",d->three_round_moves) ||
        integer(out,"three_round_search_exhaustions",d->three_round_search_exhaustions) ||
        integer(out,"three_round_nodes",d->three_round_nodes) ||
        integer(out,"changed_base_rounds",d->changed_base_rounds) ||
        integer(out,"changed_season_rounds",d->changed_season_rounds)) {
        json_free(out);return NULL;
    }
    return out;
}
static json_value_t *advanced_json(const monte_carlo_result_t *result) {
    json_value_t *out=json_new(JSON_OBJECT), *teams=json_new(JSON_ARRAY);
    if (!out || !teams)goto fail;
    size_t count=monte_carlo_result_team_count(result);
    for (size_t i=0U;i<count;++i) {
        monte_carlo_team_stats_t stats;
        char uuid[TEAM_UUID_TEXT_SIZE];
        if (monte_carlo_result_team_at(result,(team_index_t)i,&stats) ||
            team_uuid_format(&stats.uuid,uuid))goto fail;
        json_value_t *team=json_new(JSON_OBJECT), *frequencies=json_new(JSON_ARRAY);
        if(!team || !frequencies) {json_free(team);json_free(frequencies);goto fail;}
        for(uint32_t rank=1U;rank<=count;++rank) {
            size_t freq=0;
            if(monte_carlo_result_rank_frequency(result,(team_index_t)i,rank,&freq)) {
                json_free(team);json_free(frequencies);goto fail;
            }
            json_value_t *point=json_new(JSON_OBJECT);
            if(!point) {json_free(team);json_free(frequencies);goto fail;}
            if(integer(point,"rank",rank)||integer(point,"count",freq)) {
                json_free(point);json_free(team);json_free(frequencies);goto fail;
            }
            if(append(frequencies,point)) {json_free(team);json_free(frequencies);goto fail;}
        }
        if(text(team,"team_uuid",uuid)||integer(team,"observations",stats.observations)||
           nullable_number(team,"variance_championship_points",stats.has_observations,
                           stats.population_variance_points)||
           nullable_number(team,"mean_goal_difference",stats.has_observations,
                           stats.mean_goal_difference)) {
            json_free(frequencies);json_free(team);goto fail;
        }
        if(insert(team,"rank_frequencies",frequencies)) {json_free(team);goto fail;}
        if(append(teams,team))goto fail;
    }
    calendar_diagnostics_t diagnostics={0};
    if(monte_carlo_result_calendar_diagnostics(result,&diagnostics))goto fail;
    if(insert(out,"teams",teams)) {teams=NULL;goto fail;}
    teams=NULL;
    if(insert(out,"calendar_diagnostics",diagnostics_json(&diagnostics)))goto fail;
    size_t unique=0,duplicates=0;
    bool tracked=monte_carlo_result_uniqueness_tracked(result);
    if(tracked && monte_carlo_result_uniqueness(result,&unique,&duplicates))goto fail;
    json_value_t *uniq=json_new(JSON_OBJECT);
    if(!uniq)goto fail;
    if(boolean(uniq,"tracked",tracked)||
       nullable_size(uniq,"unique_calendars",tracked,unique)||
       nullable_size(uniq,"duplicate_attempts",tracked,duplicates)) {
        json_free(uniq);goto fail;
    }
    if(insert(out,"uniqueness",uniq))goto fail;
    return out;
fail:
    json_free(teams);
    json_free(out);
    return NULL;
}

static json_value_t *result_json(const monte_carlo_result_t *result,
                                 const char *view, bool csv_staged,
                                 size_t source_rounds) {
    json_value_t *out=json_new(JSON_OBJECT);
    if(!out)return NULL;
    size_t count=monte_carlo_result_team_count(result);
    if(count>SIM_MAX_TEAMS)goto fail;
    uint32_t baseline_ranks[SIM_MAX_TEAMS]={0};
    const season_resolution_t *baseline=monte_carlo_result_baseline(result);
    for(size_t i=0U;i<count;++i) {
        season_rank_entry_t e;
        if(season_resolution_rank_at(baseline,i,&e) || e.team_index>=count)goto fail;
        baseline_ranks[e.team_index]=e.sporting_rank;
    }
    const char *completion="completed";
    switch(monte_carlo_result_completion(result)) {
        case MONTE_CARLO_COMPLETED:break;
        case MONTE_CARLO_ATTEMPT_LIMIT:completion="attempt_limit";break;
        case MONTE_CARLO_CANCELLED:completion="cancelled";break;
        case MONTE_CARLO_UNIQUENESS_MEMORY_LIMIT:completion="uniqueness_memory_limit";break;
        default: goto fail;
    }
    if(text(out,"completion",completion) ||
        integer(out,"requested_samples",monte_carlo_result_requested(result)) ||
        integer(out,"attempts",monte_carlo_result_attempts(result)) ||
        integer(out,"accepted_samples",monte_carlo_result_accepted(result)) ||
        integer(out,"evaluated_rounds",monte_carlo_result_evaluated_rounds(result)) ||
        boolean(out,"provisional",monte_carlo_result_evaluated_rounds(result)!=source_rounds) ||
        insert(out,"baseline",baseline_json(baseline)) ||
        boolean(out,"observations_file",csv_staged) ||
        text(out,"statistics_view",view))goto fail;
    if(strcmp(view,"basic")!=0 && insert(out,"advanced",advanced_json(result)))goto fail;
    if(strcmp(view,"advanced")!=0 && insert(out,"basic",basic_json(result,baseline_ranks)))goto fail;
    return out;
fail:
    json_free(out);
    return NULL;
}

int simulation_cli_run(const json_value_t *request,
                       const competition_t *competition,
                       const match_rules_t *rules,
                       const monte_carlo_options_t *given,
                       const char *request_id,
                       const char *observations_destination,
                       json_value_t **out_result,
                       simulation_csv_t *out_csv,
                       simulation_error_t *out_error) {
    *out_result=NULL;
    *out_csv=(simulation_csv_t){0};
    *out_error=(simulation_error_t){0};
    const char *view=json_string(json_get(request,"statistics"));
    if(!view || (strcmp(view,"basic")!=0 && strcmp(view,"advanced")!=0 &&
                 strcmp(view,"both")!=0))
        return set_error(out_error,2,"INVALID_REQUEST", "statistics must be basic, advanced or both","/statistics");
    monte_carlo_options_t options=*given;
    uint64_t requested=0U, max_attempts=0U;
    if(uint_between(request,"requested_samples",1U,SIM_MAX_SAMPLES,&requested) ||
       uint_between(request,"max_generation_attempts",1U,SIM_MAX_ATTEMPTS,&max_attempts))
        return set_error(out_error,2,"INVALID_REQUEST", "Simulation sample/attempt budget out of range",NULL);
    options.requested_samples=(size_t)requested;
    options.max_generation_attempts=(size_t)max_attempts;
    if(opt_bool(request,"unique_only",&options.unique_only) ||
       opt_bool(request,"track_uniqueness",&options.track_uniqueness) ||
       opt_size(request,"progress_every_attempts",1U,SIM_MAX_ATTEMPTS,
                &options.progress_every_attempts))
        return set_error(out_error,2,"INVALID_REQUEST", "Invalid simulation flags or progress interval",NULL);
    bool emit=false;
    if(opt_bool(request,"emit_progress",&emit))
        return set_error(out_error,2,"INVALID_REQUEST", "emit_progress must be boolean","/emit_progress");
    if(options.unique_only && !options.track_uniqueness)
        return set_error(out_error,2,"INVALID_REQUEST", "unique_only requires track_uniqueness","/unique_only");
    if(options.track_uniqueness)options.uniqueness_memory_limit_bytes=SIM_UNIQUENESS_DEFAULT;
    if(opt_size(request,"uniqueness_memory_limit_bytes",1U,SIM_UNIQUENESS_MAX,
                &options.uniqueness_memory_limit_bytes))
        return set_error(out_error,2,"INVALID_REQUEST", "Uniqueness memory limit must be within 256 MiB","/uniqueness_memory_limit_bytes");
    if(!options.track_uniqueness && json_get(request,"uniqueness_memory_limit_bytes"))
        return set_error(out_error,2,"INVALID_REQUEST", "Memory limit requires track_uniqueness","/uniqueness_memory_limit_bytes");
    FILE *csv=NULL;
    if(observations_destination) {
        csv=stage_csv(out_csv,observations_destination);
        if(!csv) {
            simulation_csv_discard(out_csv);
            return set_error(out_error,5,"IO_ERROR", "Cannot create staged observations CSV",NULL);
        }
    }
    monte_carlo_result_t *native=NULL;
    progress_context_t progress={request_id,emit,false,SIZE_MAX,SIZE_MAX};
    stop_requested=0;
    void (*previous_int)(int)=signal(SIGINT,request_stop);
    void (*previous_term)(int)=signal(SIGTERM,request_stop);
    int status=monte_carlo_run(competition,rules,&options,csv,progress_callback,
                               &progress,&native);
    if(previous_int!=SIG_ERR)(void)signal(SIGINT,previous_int);
    if(previous_term!=SIG_ERR)(void)signal(SIGTERM,previous_term);
    if(csv && (fflush(csv)!=0 || ferror(csv)))status=-EIO;
    if(csv && fclose(csv)!=0)status=-EIO;
    if(progress.io_failed)status=-EIO;
    if(status==0 && !emit_progress(&progress,monte_carlo_result_attempts(native),
                                    monte_carlo_result_accepted(native)))status=-EIO;
    if(status) {
        monte_carlo_result_destroy(native);
        simulation_csv_discard(out_csv);
        if(status==-EAGAIN)
            return set_error(out_error,3,"PENDING_SCORES","Simulation needs a nonempty FINAL round prefix","/scope");
        if(status==-ENOTSUP)
            return set_error(out_error,3,"UNSUPPORTED_CALENDAR","Calendar violates generator constraints","/competition/rounds");
        if(status==-ENOMEM || status==-EOVERFLOW)
            return set_error(out_error,4,"RESOURCE_EXHAUSTED","Native simulation resource budget exceeded",NULL);
        if(status==-EIO)
            return set_error(out_error,5,"IO_ERROR","Simulation observation or progress stream failed",NULL);
        return set_error(out_error,3,"INVALID_SNAPSHOT","Native simulation rejected the snapshot or options",NULL);
    }
    /* The v1 response schema caps diagnostics at UINT32_MAX. Preserve its
     * strictness rather than silently emitting out-of-schema large counters. */
    if(strcmp(view,"basic")!=0) {
        calendar_diagnostics_t d={0};
        if(monte_carlo_result_calendar_diagnostics(native,&d) ||
           d.two_round_attempts>UINT32_MAX || d.two_round_partial_moves>UINT32_MAX ||
           d.two_round_whole_swaps>UINT32_MAX || d.three_round_attempts>UINT32_MAX ||
           d.three_round_moves>UINT32_MAX || d.three_round_search_exhaustions>UINT32_MAX ||
           d.three_round_nodes>UINT32_MAX || d.changed_base_rounds>UINT32_MAX ||
           d.changed_season_rounds>UINT32_MAX) {
            monte_carlo_result_destroy(native);
            simulation_csv_discard(out_csv);
            return set_error(out_error,2,"LIMIT_EXCEEDED",
                             "Simulation diagnostics exceed JSON v1 integer bounds",NULL);
        }
    }
    *out_result=result_json(native,view,csv!=NULL,competition_round_count(competition));
    monte_carlo_result_destroy(native);
    if(!*out_result) {
        simulation_csv_discard(out_csv);
        return set_error(out_error,4,"RESOURCE_EXHAUSTED","Cannot serialize simulation statistics",NULL);
    }
    return 0;
}
