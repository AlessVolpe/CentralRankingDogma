/* Only implementation and designated failure-injection tests may use this. */
#ifndef LEADERBOARD_CALENDAR_INTERNAL_H
#define LEADERBOARD_CALENDAR_INTERNAL_H
#include "calendar.h"
#include "mtwister.h"

struct calendar {
    size_t team_count, round_count;
    uint32_t *matchdays;
    fixture_t *fixtures; /* row-major; team_count/2 fixtures per round */
};

struct calendar_generator {
    const competition_t *source; /* borrowed */
    calendar_options_t options;
    mtwister_t rng; /* value-owned, standard MT19937 initialization */
    size_t n, rounds, period, base_rounds, fixtures_per_round;
    size_t frequency_group_split;
    size_t orientation_stride;
    fixture_t *initial_base;
    fixture_t *chain_base;
    uint8_t *orientation; /* (low*n + high)*stride + encounter ordinal */
    bool generated;
};
/* Internal generator moves. All operations use caller-owned working data. */
uint32_t calendar_random_bounded(mtwister_t *rng, uint32_t bound);
void calendar_shuffle_group(mtwister_t *rng, fixture_t *base, size_t first,
                            size_t length, size_t fixtures_per_round);
void calendar_shuffle_base(const calendar_generator_t *generator,
                           mtwister_t *rng, fixture_t *base);
int calendar_recombine_base(const calendar_generator_t *generator,
                            mtwister_t *rng, fixture_t *base,
                            size_t moves, calendar_diagnostics_t *stats);
#endif /* LEADERBOARD_CALENDAR_INTERNAL_H */
