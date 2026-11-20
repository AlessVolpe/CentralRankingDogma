/*
 * LeaderboardSystem JSON CLI private value tree (3.1.3).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef LEADERBOARD_JSON_VALUE_H
#define LEADERBOARD_JSON_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum json_kind { JSON_NULL, JSON_BOOLEAN, JSON_INTEGER,
                         JSON_DECIMAL, JSON_STRING, JSON_ARRAY, JSON_OBJECT } json_kind_t;
typedef struct json_value json_value_t;

/* Strict UTF-8, duplicate-key-free JSON, bounded recursion (64 levels).
 * 0 success. Invalid syntax, invalid UTF-8, surrogate errors and overflow
 * return -1. Allocation failure returns -2. */
int json_parse(const char *input, size_t bytes, json_value_t **out);
void json_free(json_value_t *value);
json_kind_t json_kind(const json_value_t *value);
const json_value_t *json_get(const json_value_t *object, const char *name);
const json_value_t *json_at(const json_value_t *array, size_t index);
size_t json_count(const json_value_t *value);
const char *json_string(const json_value_t *value);
int json_uint(const json_value_t *value, uint64_t *out);
int json_boolean(const json_value_t *value, bool *out);
bool json_has_only(const json_value_t *object, const char *const *keys,
                   size_t key_count);

json_value_t *json_new(json_kind_t kind);
json_value_t *json_text(const char *value);
json_value_t *json_number(uint64_t value);
json_value_t *json_signed(int64_t value);
json_value_t *json_bool(bool value);
int json_push(json_value_t *array, json_value_t *value);
int json_put(json_value_t *object, const char *key, json_value_t *value);
/* Returns a heap-allocated NUL-terminated JSON document, or NULL on error.
 * The caller frees it. Output has no locale-dependent numeric representation. */
char *json_serialize(const json_value_t *value, size_t max_bytes);

#endif
