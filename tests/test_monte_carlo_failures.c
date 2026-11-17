/* Allocation-failure transaction checks for the 3.1.1 batch engine. */
#include "monte_carlo.h"
#include "calendar_test_fixture.h"
#include "alloc_internal.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}}while(0)

int main(void) {
    calendar_test_fixture_t input;
    CHECK(calendar_test_prepare(&input, 4U, 3U) == 0);
    competition_t *c = NULL;
    CHECK(competition_create(&input.input, &c) == 0);
    match_rules_t rules;
    CHECK(match_rules_default(&rules) == 0);
    monte_carlo_options_t options = monte_carlo_options_default();
    options.calendar.method = CALENDAR_METHOD_ROUND_SHUFFLE;
    options.requested_samples = options.max_generation_attempts = 1U;
    options.track_uniqueness = true;
    options.uniqueness_memory_limit_bytes = 1024U * 1024U;
    size_t caught = 0U;
    bool completed = false;
    for (size_t i = 0U; i < 160U; ++i) {
        monte_carlo_result_t *result = (monte_carlo_result_t *)(uintptr_t)1U;
        leaderboard_alloc_fail_after(i);
        int status = monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&result);
        leaderboard_alloc_reset();
        if (status == -ENOMEM) {
            CHECK(result == NULL);
            caught++;
        } else {
            CHECK(status == 0 && result != NULL);
            CHECK(monte_carlo_result_accepted(result) == 1U);
            monte_carlo_result_destroy(result);
            completed = true;
            break;
        }
    }
    CHECK(completed && caught >= 10U);
    monte_carlo_result_t *result = (monte_carlo_result_t *)(uintptr_t)1U;
    options.unique_only = true;
    options.track_uniqueness = false;
    CHECK(monte_carlo_run(c,&rules,&options,NULL,NULL,NULL,&result) == -EINVAL);
    CHECK(result == NULL);
    competition_destroy(c);
    return EXIT_SUCCESS;
}
