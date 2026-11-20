/* LeaderboardSystem 3.1.1 public API, reproducibility and research semantics. */
#include "monte_carlo.h"
#include "calendar_test_fixture.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(p) do { if (!(p)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p); return EXIT_FAILURE; } } while(0)

typedef struct cancel_context { size_t calls; size_t last_attempt; } cancel_context_t;
static bool cancel_after_three(const monte_carlo_progress_t *info, void *context) {
    cancel_context_t *c = context;
    c->calls++;
    c->last_attempt = info->generation_attempts;
    return info->generation_attempts < 3U;
}

static int reproducibility_and_csv(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input, 8U, 14U) == 0);
    competition_t *c = NULL;
    CHECK(competition_create(&input.input, &c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    monte_carlo_options_t options = monte_carlo_options_default();
    options.requested_samples = options.max_generation_attempts = 30U;
    options.calendar.seed = 12345U;
    options.calendar.method = CALENDAR_METHOD_RECOMBINE;
    FILE *raw = tmpfile();
    CHECK(raw != NULL);
    monte_carlo_result_t *a = NULL, *b = NULL;
    CHECK(monte_carlo_run(c, &rules, &options, raw, NULL, NULL, &a) == 0);
    CHECK(monte_carlo_run(c, &rules, &options, NULL, NULL, NULL, &b) == 0);
    CHECK(monte_carlo_result_completion(a) == MONTE_CARLO_COMPLETED);
    CHECK(monte_carlo_result_evaluated_rounds(a) == 8U);
    CHECK(monte_carlo_result_accepted(a) == 30U);
    CHECK(monte_carlo_result_attempts(a) == 30U);
    CHECK(!monte_carlo_result_uniqueness_tracked(a));
    size_t unique, dup;
    CHECK(monte_carlo_result_uniqueness(a, &unique, &dup) == -ENOTSUP);
    const season_resolution_t *baseline = monte_carlo_result_baseline(a);
    CHECK(baseline != NULL && season_resolution_round_count(baseline) == 8U);
    for (size_t i = 0; i < 8U; ++i) {
        monte_carlo_team_stats_t x, y;
        CHECK(monte_carlo_result_team_at(a, (team_index_t)i, &x) == 0);
        CHECK(monte_carlo_result_team_at(b, (team_index_t)i, &y) == 0);
        CHECK(x.has_observations && x.observations == 30U);
        CHECK(x.mean_points == y.mean_points);
        CHECK(x.population_variance_points == y.population_variance_points);
        CHECK(x.mean_goal_difference == y.mean_goal_difference);
        CHECK(x.first_place_finishes == y.first_place_finishes);
        CHECK(x.population_variance_points >= -0.0000001);
        size_t count = 0U;
        for (uint32_t rank = 1U; rank <= 8U; ++rank) {
            size_t xa, xb;
            CHECK(monte_carlo_result_rank_frequency(a, (team_index_t)i, rank, &xa) == 0);
            CHECK(monte_carlo_result_rank_frequency(b, (team_index_t)i, rank, &xb) == 0);
            CHECK(xa == xb);
            count += xa;
        }
        CHECK(count == 30U);
    }
    CHECK(fseek(raw, 0L, SEEK_SET) == 0);
    char line[512];
    CHECK(fgets(line, sizeof(line), raw) != NULL);
    CHECK(strstr(line, "evaluated_directed_fingerprint") != NULL);
    size_t row_count = 0U;
    while (fgets(line, sizeof(line), raw) != NULL) row_count++;
    CHECK(row_count == 30U * 8U);
    CHECK(fclose(raw) == 0);
    FILE *summary = tmpfile();
    CHECK(summary != NULL);
    CHECK(monte_carlo_write_summary_csv(a, summary) == 0);
    CHECK(fseek(summary,0L,SEEK_SET) == 0);
    CHECK(fgets(line,sizeof(line),summary) != NULL);
    CHECK(strstr(line,"population_variance_points") != NULL);
    CHECK(fclose(summary) == 0);
    monte_carlo_result_destroy(a);
    monte_carlo_result_destroy(b);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

static int duplicates_budget_and_cancel(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input,4U,3U) == 0);
    competition_t *c = NULL;
    CHECK(competition_create(&input.input, &c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    monte_carlo_options_t options = monte_carlo_options_default();
    options.calendar.method = CALENDAR_METHOD_ROUND_SHUFFLE;
    options.calendar.seed = 1301U;
    options.requested_samples = 20U;
    options.max_generation_attempts = 150U;
    options.track_uniqueness = true;
    options.unique_only = true;
    options.uniqueness_memory_limit_bytes = 1024U * 1024U;
    monte_carlo_result_t *r = NULL;
    CHECK(monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&r) == 0);
    CHECK(monte_carlo_result_completion(r) == MONTE_CARLO_ATTEMPT_LIMIT);
    CHECK(monte_carlo_result_attempts(r) == 150U);
    size_t distinct, duplicates;
    CHECK(monte_carlo_result_uniqueness(r,&distinct,&duplicates) == 0);
    CHECK(distinct == monte_carlo_result_accepted(r));
    CHECK(distinct >= 1U && distinct <= 6U);
    CHECK(distinct + duplicates == 150U);
    monte_carlo_result_destroy(r);

    options.uniqueness_memory_limit_bytes = 1U;
    r = (monte_carlo_result_t *)(uintptr_t)1U;
    CHECK(monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&r) == 0);
    CHECK(monte_carlo_result_completion(r) == MONTE_CARLO_UNIQUENESS_MEMORY_LIMIT);
    CHECK(monte_carlo_result_accepted(r) == 0U);
    CHECK(monte_carlo_result_attempts(r) == 1U);
    monte_carlo_team_stats_t empty;
    CHECK(monte_carlo_result_team_at(r,0U,&empty) == 0 && !empty.has_observations);
    monte_carlo_result_destroy(r);

    options.unique_only = options.track_uniqueness = false;
    options.uniqueness_memory_limit_bytes = 0U;
    options.requested_samples = options.max_generation_attempts = 20U;
    options.progress_every_attempts = 1U;
    cancel_context_t context = {0};
    CHECK(monte_carlo_run(c,&rules,&options,NULL,cancel_after_three,&context,&r) == 0);
    CHECK(monte_carlo_result_completion(r) == MONTE_CARLO_CANCELLED);
    CHECK(monte_carlo_result_accepted(r) == 3U);
    CHECK(context.calls == 3U && context.last_attempt == 3U);
    monte_carlo_result_destroy(r);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

static int empty_prefix_and_home_advantage(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input,10U,35U) == 0);
    for (size_t i = 0U; i < 350U; ++i)
        input.scores[i].score = (fantasy_score_t){660U,FANTASY_SCORE_FINAL};
    competition_t *c = NULL;
    CHECK(competition_create(&input.input, &c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    rules.home_advantage_enabled = true;
    rules.home_advantage_tenths = 40U;
    rules.home_advantage_exclude_final_cycle = true;
    season_resolution_t *baseline = NULL;
    CHECK(season_resolve(c,NULL,&rules,SEASON_RESOLVE_FULL_SEASON,&baseline) == 0);
    CHECK(season_resolution_round_count(baseline) == 35U);
    for (size_t i = 0; i < 175U; ++i) {
        season_fixture_result_t e;
        CHECK(season_resolution_fixture_at(baseline,i,&e) == 0);
        CHECK(e.result.home_fantasy_tenths == 660U);
        CHECK(e.result.home_goals == (i/5U < 27U ? 2U : 1U));
        CHECK(e.result.away_goals == 1U);
    }
    season_resolution_destroy(baseline);
    monte_carlo_options_t options = monte_carlo_options_default();
    options.scope = SEASON_RESOLVE_FULL_SEASON;
    options.requested_samples = options.max_generation_attempts = 2U;
    monte_carlo_result_t *result = NULL;
    CHECK(monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&result) == 0);
    CHECK(monte_carlo_result_completion(result) == MONTE_CARLO_COMPLETED);
    CHECK(monte_carlo_result_evaluated_rounds(result) == 35U);
    monte_carlo_result_destroy(result);
    competition_destroy(c);

    CHECK(calendar_test_prepare(&input,4U,3U) == 0);
    input.scores[0].score = (fantasy_score_t){0U,FANTASY_SCORE_PENDING};
    CHECK(competition_create(&input.input,&c) == 0);
    result = (monte_carlo_result_t *)(uintptr_t)1U;
    options.scope = SEASON_RESOLVE_FINALIZED_PREFIX;
    CHECK(monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&result) == -EAGAIN);
    CHECK(result == NULL);
    competition_destroy(c);
    return EXIT_SUCCESS;
}

int main(void) {
    CHECK(reproducibility_and_csv() == 0);
    CHECK(duplicates_budget_and_cancel() == 0);
    CHECK(empty_prefix_and_home_advantage() == 0);
    return EXIT_SUCCESS;
}
