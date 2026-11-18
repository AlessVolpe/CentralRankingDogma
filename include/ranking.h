/*
 * LeaderboardSystem ranking abstraction (milestone 3.0.5).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LEADERBOARD_RANKING_H
#define LEADERBOARD_RANKING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque, single-threaded ranking. Scores are signed, exact integers. */
typedef struct ranking ranking_t;

/* Custom keys are opaque, fixed-size byte sequences copied by the ranking.
 * compare(a,b,context) must implement a stable strict total preorder on the
 * key VALUES: negative if a sorts first, zero if equivalent, positive if b
 * sorts first. The ranking applies strcmp(id) as the final deterministic
 * tiebreak. No comparator callbacks may mutate/reenter the ranking.
 * context is borrowed, can be NULL, and must outlive ranking_destroy().
 * The key layout and size must stay fixed for the entire ranking lifetime.
 * A custom ranking has zero-based physical positions even for equal keys;
 * domain-specific shared sporting positions must be computed separately. */
typedef int (*ranking_key_compare_fn)(const void *a, const void *b, void *context);

/* Returns NULL for key_size == 0, compare == NULL, or allocation failure.
 * A custom ranking rejects the legacy scalar operations with -EINVAL. */
ranking_t *ranking_new_custom(size_t key_size, ranking_key_compare_fn compare,
                              void *context);

/* Both operations copy key_size bytes, never borrowing the caller's key.
 * The first insertion owns a copy of id; updates preserve id pointer identity.
 * Equal comparator values may update stored key bytes in place without
 * changing the order. Failed updates preserve the previous key and rank.
 * Return 0, -EINVAL, -ENOMEM, or -EOVERFLOW as appropriate. */
int ranking_upsert_key(ranking_t *ranking, const char *id, const void *key);

/* key_out is optional; if not NULL it must point to at least key_size writable
 * bytes and receives a COPY. rank is optional and zero-based. In the select
 * operation id is a borrowed pointer with the same lifetime as ranking_at.
 * Return 0, -EINVAL for wrong mode/arguments, -ENOENT if not found. */
int ranking_get_key(const ranking_t *ranking, const char *id,
                    void *key_out, size_t *rank);
int ranking_at_key(const ranking_t *ranking, size_t rank,
                   const char **id, void *key_out);

/* Returns NULL on allocation failure. */
ranking_t *ranking_new(void);

/* NULL is accepted. Invalidates all borrowed identifiers. */
void ranking_destroy(ranking_t *ranking);

/* Number of unique identifiers; NULL returns zero. */
size_t ranking_size(const ranking_t *ranking);

/* Insert or replace a score for a nonempty, NUL-terminated identifier.
 * The identifier is copied when first inserted, and is unchanged on updates.
 * Ordering: score descending, then bytewise strcmp(id) ascending.
 * Returns 0 on success; -EINVAL for invalid arguments, -ENOMEM on allocation
 * failure, or -EOVERFLOW if the underlying tree cannot grow. An unsuccessful
 * update leaves the previous score and rank intact. No-op updates allocate
 * nothing. The order-statistics tree uses zero-based ranks. */
int ranking_upsert(ranking_t *ranking, const char *id, int64_t score);

/* Remove an existing identifier. Returns 0, -EINVAL or -ENOENT. */
int ranking_remove(ranking_t *ranking, const char *id);

/* Query an identifier in expected O(1) + worst-case O(log n) time.
 * score and rank are optional outputs. Returns 0, -EINVAL, or -ENOENT.
 * rank is zero-based (0 is best). */
int ranking_get(const ranking_t *ranking, const char *id,
                int64_t *score, size_t *rank);

/* Select an entry by zero-based rank in O(log n) time.
 * id and score are optional outputs. The returned id is borrowed: it stays
 * valid across score updates, but not after removal or ranking_destroy().
 * Returns 0, -EINVAL, or -ENOENT for an out-of-range rank. */
int ranking_at(const ranking_t *ranking, size_t rank,
               const char **id, int64_t *score);

#ifdef __cplusplus
}
#endif

#endif /* LEADERBOARD_RANKING_H */
