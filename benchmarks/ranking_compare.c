/*
 * LeaderboardSystem comparable composite-key ranking microbenchmarks (3.1.2).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Use Release builds and process isolation for performance results. CI uses
 * this runner solely for correctness / output-schema smoke checks.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "ranking.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_ENTRIES 100000U
#define ID_BYTES 24U

typedef struct sporting_key {
    uint32_t points;
    uint32_t fantasy;
    int64_t goal_difference;
} sporting_key_t;

typedef struct datum {
    sporting_key_t key;
    char id[ID_BYTES];
    uint32_t index;
} datum_t;

typedef struct array_ranking {
    uint32_t *order;
    size_t *positions;
    size_t size;
} array_ranking_t;

static volatile uint64_t benchmark_sink;
static void require(int condition, const char *message) {
    if (!condition) { fprintf(stderr, "ranking_compare: %s\n", message); exit(EXIT_FAILURE); }
}
static uint32_t random32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *state = x;
}
static int compare_key(const void *a, const void *b, void *ignored) {
    const sporting_key_t *x = a, *y = b;
    (void)ignored;
    if (x->points != y->points) return x->points > y->points ? -1 : 1;
    if (x->fantasy != y->fantasy) return x->fantasy > y->fantasy ? -1 : 1;
    if (x->goal_difference != y->goal_difference)
        return x->goal_difference > y->goal_difference ? -1 : 1;
    return 0;
}
static int compare_record(const datum_t *data, uint32_t x, uint32_t y) {
    int c = compare_key(&data[x].key, &data[y].key, NULL);
    return c != 0 ? c : strcmp(data[x].id, data[y].id);
}
static double seconds_wall(void) {
#ifdef _WIN32
    LARGE_INTEGER ticks, freq;
    require(QueryPerformanceCounter(&ticks) && QueryPerformanceFrequency(&freq) && freq.QuadPart > 0,
            "QueryPerformanceCounter unavailable");
    return (double)ticks.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    require(clock_gettime(CLOCK_MONOTONIC, &ts) == 0, "CLOCK_MONOTONIC unavailable");
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}
static double seconds_cpu(void) {
    clock_t t = clock();
    require(t != (clock_t)-1, "clock() unavailable");
    return (double)t / (double)CLOCKS_PER_SEC;
}
static void report(const char *backend, const char *phase, size_t n, size_t ops,
                   unsigned replication, uint32_t seed, double wall, double cpu,
                   uint64_t checksum) {
    benchmark_sink ^= checksum + UINT64_C(0x9e3779b97f4a7c15);
    printf("ranking,synthetic,%s,%s,%zu,%zu,%u,%" PRIu32 ",%.9f,%.9f,%" PRIu64 "\n",
           backend, phase, n, ops, replication, seed, wall, cpu, checksum);
}
static void shuffle(uint32_t *order, size_t n, uint32_t *seed) {
    for (size_t i = 0; i < n; ++i) order[i] = (uint32_t)i;
    for (size_t i = n; i > 1U; --i) {
        size_t j = random32(seed) % i;
        uint32_t tmp = order[i - 1U]; order[i - 1U] = order[j]; order[j] = tmp;
    }
}
static void init_data(datum_t *data, size_t n, uint32_t *seed) {
    for (size_t i = 0; i < n; ++i) {
        uint32_t r = random32(seed);
        int len = snprintf(data[i].id, ID_BYTES, "team-%08" PRIx32, (uint32_t)i);
        require(len > 0 && len < (int)ID_BYTES, "ID formatting");
        data[i].index = (uint32_t)i;
        data[i].key.points = r % 40U;
        data[i].key.fantasy = 500U + (r >> 8) % 500U;
        data[i].key.goal_difference = (int64_t)(int32_t)(r % 51U) - 25;
    }
}
/* Reference heapsort, same negative-is-earlier semantics as season resolver. */
static void sift(uint32_t *v, size_t root, size_t end, const datum_t *data) {
    while (root < end / 2U) {
        size_t child = 2U * root + 1U, late = root;
        if (compare_record(data, v[late], v[child]) < 0) late = child;
        if (child + 1U < end && compare_record(data, v[late], v[child + 1U]) < 0)
            late = child + 1U;
        if (late == root) return;
        uint32_t tmp = v[root]; v[root] = v[late]; v[late] = tmp;
        root = late;
    }
}
static void heap_sort(uint32_t *v, size_t n, const datum_t *data) {
    for (size_t i = n / 2U; i > 0U; --i) sift(v, i - 1U, n, data);
    for (size_t end = n; end > 1U; --end) {
        uint32_t tmp = v[0]; v[0] = v[end - 1U]; v[end - 1U] = tmp;
        sift(v, 0U, end - 1U, data);
    }
}
static size_t insertion_position(const array_ranking_t *a, const datum_t *data,
                                 uint32_t id) {
    size_t low = 0U, high = a->size;
    while (low < high) {
        size_t mid = low + (high - low) / 2U;
        if (compare_record(data, a->order[mid], id) < 0) low = mid + 1U;
        else high = mid;
    }
    return low;
}
static void array_insert(array_ranking_t *a, const datum_t *data, uint32_t id) {
    require(a->positions[id] == SIZE_MAX, "array duplicate insert");
    size_t at = insertion_position(a, data, id);
    memmove(&a->order[at + 1U], &a->order[at], (a->size - at) * sizeof(*a->order));
    ++a->size;
    a->order[at] = id;
    for (size_t i = at; i < a->size; ++i) a->positions[a->order[i]] = i;
}
static void array_remove(array_ranking_t *a, uint32_t id) {
    size_t at = a->positions[id];
    require(at != SIZE_MAX && at < a->size, "array missing remove");
    memmove(&a->order[at], &a->order[at + 1U], (a->size - at - 1U) * sizeof(*a->order));
    --a->size;
    a->positions[id] = SIZE_MAX;
    for (size_t i = at; i < a->size; ++i) a->positions[a->order[i]] = i;
}
static uint64_t check_array(const array_ranking_t *a, const datum_t *data) {
    uint64_t sum = 0U;
    for (size_t i = 0; i < a->size; ++i) {
        if (i) require(compare_record(data, a->order[i-1U], a->order[i]) < 0,
                       "array not strictly sorted");
        require(a->positions[a->order[i]] == i, "array position mismatch");
        sum += (uint64_t)(i + 1U) * (uint64_t)(a->order[i] + 1U);
    }
    return sum;
}
static uint64_t check_avl(const ranking_t *ranking, const array_ranking_t *a,
                          const datum_t *data) {
    require(ranking_size(ranking) == a->size, "AVL size mismatch");
    uint64_t sum = 0U;
    for (size_t i = 0; i < a->size; ++i) {
        const char *id = NULL;
        sporting_key_t key;
        size_t position = SIZE_MAX;
        uint32_t index = a->order[i];
        require(ranking_at_key(ranking, i, &id, &key) == 0 &&
                strcmp(id, data[index].id) == 0 &&
                compare_key(&key, &data[index].key, NULL) == 0,
                "AVL select differs from array reference");
        require(ranking_get_key(ranking, id, NULL, &position) == 0 && position == i,
                "AVL lookup rank differs from array reference");
        sum += (uint64_t)(i + 1U) * (uint64_t)(index + 1U);
    }
    return sum;
}
static size_t positive(const char *text, size_t max) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    require(errno == 0 && end != text && *end == '\0' && text[0] != '-' &&
            value > 0U && value <= max, "invalid positive parameter");
    return (size_t)value;
}
static uint32_t parse_seed(const char *text) {
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    require(errno == 0 && end != text && *end == '\0' && text[0] != '-' &&
            value > 0U && value <= UINT32_MAX, "invalid seed");
    return (uint32_t)value;
}
static uint64_t measure_queries_array(const array_ranking_t *a, const datum_t *data, const uint32_t *order,
                                      size_t n, size_t count) {
    uint64_t sum = 0U;
    for (size_t i = 0; i < count; ++i) {
        uint32_t id = order[i % n];
        require(a->positions[id] != SIZE_MAX, "array missing query");
        sum += a->positions[id] + (unsigned char)data[a->order[i % a->size]].id[0];
    }
    return sum;
}
static uint64_t measure_queries_avl(const ranking_t *rank, const datum_t *data,
                                    const uint32_t *order, size_t n, size_t count) {
    uint64_t sum = 0U;
    for (size_t i = 0; i < count; ++i) {
        uint32_t id = order[i % n];
        size_t at = SIZE_MAX;
        const char *selected = NULL;
        require(ranking_get_key(rank, data[id].id, NULL, &at) == 0, "AVL missing query");
        require(ranking_at_key(rank, i % ranking_size(rank), &selected, NULL) == 0,
                "AVL missing select");
        sum += at + (unsigned char)selected[0];
    }
    return sum;
}
static void run(size_t n, size_t incremental_ops, unsigned repetition, uint32_t base_seed) {
    uint32_t seed = base_seed ^ (uint32_t)(repetition * UINT32_C(0x9e3779b9));
    if (seed == 0U) seed = 1U;
    datum_t *data = calloc(n, sizeof(*data));
    uint32_t *permutation = malloc(n * sizeof(*permutation));
    uint32_t *heap = malloc(n * sizeof(*heap));
    uint32_t *ordered = malloc(n * sizeof(*ordered));
    size_t *positions = malloc(n * sizeof(*positions));
    require(data && permutation && heap && ordered && positions, "workload allocation");
    init_data(data, n, &seed);
    shuffle(permutation, n, &seed);
    for (size_t i = 0; i < n; ++i) positions[i] = SIZE_MAX;
    array_ranking_t arr = {ordered, positions, 0U};
    memcpy(heap, permutation, n * sizeof(*heap));
    double wall = seconds_wall(), cpu = seconds_cpu();
    heap_sort(heap, n, data);
    double elapsed_wall = seconds_wall() - wall, elapsed_cpu = seconds_cpu() - cpu;
    report("heapsort", "sort", n, n, repetition, base_seed, elapsed_wall, elapsed_cpu, 0U);
    /* Index-building is separately measured because the tree already includes its lookup index. */
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < n; ++i) positions[heap[i]] = i;
    report("heapsort", "id_index", n, n, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    uint64_t reference = 0U;
    for (size_t i = 0; i < n; ++i)
        reference += (uint64_t)(i + 1U) * (uint64_t)(heap[i] + 1U);
    arr.size = n;
    memcpy(arr.order, heap, n * sizeof(*heap));
    require(check_array(&arr, data) == reference, "reference checksum");
    ranking_t *tree = ranking_new_custom(sizeof(sporting_key_t), compare_key, NULL);
    require(tree != NULL, "AVL construction");
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < n; ++i) {
        uint32_t id = permutation[i];
        require(ranking_upsert_key(tree, data[id].id, &data[id].key) == 0, "AVL insert");
    }
    report("avl", "construct", n, n, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    require(check_avl(tree, &arr, data) == reference, "reference mismatch");
    size_t query_count = n < 1024U ? 1024U : n;
    wall = seconds_wall(); cpu = seconds_cpu();
    uint64_t sum = measure_queries_array(&arr, data, permutation, n, query_count);
    report("heapsort", "get_and_select", n, query_count, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, sum);
    wall = seconds_wall(); cpu = seconds_cpu();
    sum = measure_queries_avl(tree, data, permutation, n, query_count);
    report("avl", "get_and_select", n, query_count, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, sum);
    /* Incremental array baseline begins from the same sorted snapshot.
     * Do not artificially penalize it with quadratic initial construction. */
    wall = seconds_wall(); cpu = seconds_cpu();
    memcpy(arr.order, heap, n * sizeof(*heap));
    for (size_t i = 0; i < n; ++i) arr.positions[arr.order[i]] = i;
    arr.size = n;
    report("sorted_array", "initialize_from_sorted", n, n, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    require(check_array(&arr, data) == reference, "sorted array differs from heapsort");
    require(check_avl(tree, &arr, data) == reference, "AVL differs from sorted array");
    /* Incremental update: remove/reinsert in the sorted array, atomic
     * replacement in the AVL. Both process precisely the same key changes. */
    sporting_key_t *updates = malloc(incremental_ops * sizeof(*updates));
    require(updates != NULL, "update trace allocation");
    for (size_t i = 0; i < incremental_ops; ++i) {
        uint32_t id = permutation[i];
        updates[i] = data[id].key;
        updates[i].points = (updates[i].points + 17U + (uint32_t)i % 23U) % 60U;
        updates[i].goal_difference += (int64_t)(i % 7U) - 3;
    }
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i) {
        uint32_t id = permutation[i];
        array_remove(&arr, id);
        data[id].key = updates[i];
        array_insert(&arr, data, id);
    }
    report("sorted_array", "update", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i) {
        uint32_t id = permutation[i];
        require(ranking_upsert_key(tree, data[id].id, &updates[i]) == 0, "AVL update");
    }
    report("avl", "update", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    free(updates);
    require(check_avl(tree, &arr, data) == check_array(&arr, data), "incremental mismatch");
    /* Deletion: same recorded identifiers, same initial state. */
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i)
        array_remove(&arr, permutation[i]);
    report("sorted_array", "delete", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i)
        require(ranking_remove(tree, data[permutation[i]].id) == 0, "AVL delete");
    report("avl", "delete", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    require(check_avl(tree, &arr, data) == check_array(&arr, data), "delete mismatch");
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i)
        array_insert(&arr, data, permutation[i]);
    report("sorted_array", "insert", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    wall = seconds_wall(); cpu = seconds_cpu();
    for (size_t i = 0; i < incremental_ops; ++i) {
        uint32_t id = permutation[i];
        require(ranking_upsert_key(tree, data[id].id, &data[id].key) == 0, "AVL reinsert");
    }
    report("avl", "insert", n, incremental_ops, repetition, base_seed,
           seconds_wall() - wall, seconds_cpu() - cpu, 0U);
    require(check_avl(tree, &arr, data) == check_array(&arr, data), "reinsert mismatch");

    ranking_destroy(tree);
    free(positions); free(ordered); free(heap); free(permutation); free(data);
}
int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <count 2..100000> <repetitions> <seed> <incremental_ops>\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t n = positive(argv[1], MAX_ENTRIES);
    require(n >= 2U, "at least two entries required");
    size_t reps = positive(argv[2], 1000U);
    uint32_t seed = parse_seed(argv[3]);
    size_t updates = positive(argv[4], n);
    puts("experiment,dataset,backend,phase,count,operations,replication,seed,wall_seconds,cpu_seconds,checksum");
    for (size_t i = 0; i < reps; ++i) run(n, updates, (unsigned)i, seed);
    fprintf(stderr, "benchmark sink=%" PRIu64 "\n", benchmark_sink);
    return 0;
}
