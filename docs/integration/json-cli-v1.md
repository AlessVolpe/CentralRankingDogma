# Central Ranking Dogma JSON CLI v1: external integration contract

**Status:** Implemented 3.1.3 JSON CLI v1 contract, including native
`validate`, `generate`, `resolve`, and `simulate` operations, structured progress,
CSV export, cooperative cancellation, and independent Python subprocess
consumption. Milestone 3.1.4 PR B renamed the executable to `crd-json` without
changing wire schema `1.0`; PR C reorganizes source paths without protocol
changes. No standalone native Docker image is planned. Fixture response
examples are illustrative shape checks, not native golden results.

## 1. Purpose and ownership

The future Python web application owns authentication, team registration, UUID
creation, database persistence, authorized Fantagazzetta source import,
background jobs, request timeouts and displaying decimal fantasy scores.
An installed `crd-json` executable (Linux production target; Windows CI also supported) supplies stateless,
validated access to the existing ISO C11 computational library. No Python,
JSON, container or database dependency is introduced in the installed
`CentralRankingDogma::central_ranking_dogma` static library. Windows remains a CI/test target.

- The CLI accepts one JSON document from **stdin**, terminated by EOF.
- It emits exactly one complete JSON envelope followed by `\n` to **stdout**;
  there is no partial-success document, human-facing prose or mixed CSV stream.
- Optional progress arrives as one JSON object per line on **stderr**.
  Consumers must not interpret stderr as stdout or assume events are delivered
  when progress is disabled.
- Each subprocess handles **one request**. Repeated requests spawn new processes.
  No persistent generator, pointer identity, hidden session or DB state exists.
- The Python caller controls the subprocess lifetime, arguments, temporary
  files, container limits and process reaping. No web server is implemented here.

Milestone 3.1.3 originally installed `leaderboard-json`; milestone 3.1.4
PR B renames it to `crd-json` under the CMake runtime bindir (normally
`bin`), and provides a standalone Python subprocess consumer. The command
`crd-json --version` exits zero and prints
`crd-json 3.1.3` followed by LF to stdout, with empty stderr and no
stdin request. It is the **only** non-JSON stdout success mode. The JSON wire
schema remains `1.0` independent of the native 3.1.3 package version.
Containerization belongs to the future Python worker, not the standalone
native executable.

## 2. Versioning, schemas and serialization

The external protocol version is **`"1.0"`**, deliberately independent of
the planned native 3.1.3 release and subsequent native versions.
The accepted JSON documents are defined by Draft 2020-12 schemas:

- [Request schema](../../schemas/leaderboard-cli-v1-request.schema.json)
- [Response schema](../../schemas/leaderboard-cli-v1-response.schema.json)
- [Progress schema](../../schemas/leaderboard-cli-v1-progress.schema.json)
- [Illustrative fixtures](../../tests/fixtures/json_cli_v1/validate.request.json)

A request includes `schema_version`, `request_id`, `operation` and a complete
`competition` snapshot. Responses include `schema_version`, the echoed
`request_id` (or `null` if it could not be parsed), `engine` information and
`ok`. Successful responses include `operation` and its typed `result`; failures
include an `error` object. The `engine` object has a native `version` and a
build-specific `id`. If the engine is unavailable during early startup, the
implementation must still return a stable compiled-in engine identity.

JSON keys, object layout, result array ordering and output number formatting
must be deterministic for the **same executable build and request**. The
serializer emits UTF-8, stable documented key order, no NaN/Infinity and no
locale-dependent decimal separators. Whitespace outside strings is irrelevant;
byte-for-byte identity across different C implementations or compiler builds
is **not** promised. Unknown properties are rejected at every schema-defined
object level. Duplicate JSON object member names are rejected by the parser;
JSON Schema alone cannot enforce that constraint. Trailing JSON data is an error.

Schema compatibility is strict for `1.0`. An unknown request version must
return `INVALID_SCHEMA_VERSION`, not silently apply an arbitrary fallback.
Breaking field changes require a new schema version. An implementation may
add later optional fields only through a separately documented compatible
version, not by relaxing `additionalProperties: false` in-place.

## 3. Competition snapshot and score representation

`competition` contains `teams`, `rounds` and `scores`:

| Element | Meaning | Invariant |
| --- | --- | --- |
| `teams[]` | `{uuid, name}` | Even number, unique RFC 9562-layout UUIDs; name at most 255 **UTF-8 bytes** |
| `rounds[]` | `{serie_a_matchday, fixtures[]}` | Strictly increasing positive mapped days; each team appears once per round |
| `fixtures[]` | `{home_uuid, away_uuid}` | Directed, distinct and known teams; complete original schedule |
| `scores[]` | `{team_uuid, serie_a_matchday, points_tenths, status}` | Exactly one score per team and imported round; `FINAL` or `PENDING` |

UUID parsing follows [`team_uuid_parse`](../../native/include/team.h): the textual
8-4-4-4-12 format is case-insensitive and does not require UUIDv4. Canonical
JSON output uses lowercase UUIDs and stable sorting by UUID bytes.

**Wire values are integer tenths.** `665` means a user-facing `66.5` fantasy
points. Python uses `decimal.Decimal` for input conversion, rejects values not
representable in tenths, and formats `665` as `66.5` in the UI. The wire
interface must **never** use JSON floating-point `66.5` as a score.
`PENDING` cells require `points_tenths: 0`; this zero is a placeholder for
missing results, not a finalized zero. `FINAL` zero is legitimate.

The JSON Schema checks types, ranges, required properties and pending-zero
shape. The native importer must additionally enforce UTF-8 byte length,
canonical UUID uniqueness, unique mapped days, score-matrix completeness,
team participation, self-fixture rejection, checked arithmetic and all
[`competition_create`](../../native/include/competition.h) invariants. Schema validity
alone does **not** certify a runnable season. If an original schedule satisfies
the competition snapshot contract but not the generator's strict
no-early-rematch constraint, `validate` and original-season resolution may
succeed while `generate` and `simulate` return `UNSUPPORTED_CALENDAR`.

### 3.1 Scoring rules

The optional `rules` object is **all-or-nothing**: omit it to call
`match_rules_default()`; otherwise supply every existing field in
[`match_rules_t`](../../native/include/match.h): the eight ordered goal thresholds,
post-eighth step, victory limitation and subthreshold flags, draw limitation
and subthreshold flags, home-advantage enable/value and final-cycle exclusion.
All score thresholds and adjustments use integer tenths. The C library
validates threshold ordering and enabled modifier constraints. Unknown rule
names, silently substituted defaults for partial rules and lossy conversions
are errors.

## 4. Operations

| `operation` | Required operation-specific fields | Result |
| --- | --- | --- |
| `validate` | None beyond `competition` | Counts, `simulation_ready`, maximal `finalized_prefix_rounds` |
| `generate` | `generator`, `sample_index` | Transient round fixtures and reproducibility `replay` recipe |
| `resolve` | `scope`, `calendar` | Evaluated fixture outcomes and sporting standings |
| `simulate` | `scope`, `generator`, `requested_samples`, `max_generation_attempts`, `statistics` | Baseline, completion status and requested basic/advanced statistics |

`generator` requires explicit `method` (`shuffle`/`recombine`), `sampling_mode`
(`independent`/`chain`) and 32-bit `seed`. The remaining generation options are
optional in a **generation request**: absent values are populated by [`calendar_options_default`](../../native/include/calendar.h)
and the complete effective configuration is echoed in a replay recipe.
Requesting `sample_index: 3` means creating **three successive** calendars in
one generator instance and returning the **third**, not computing a calendar
from seed + 3. This distinction matters especially for chain sampling.
Sample indices are 1-based. The generated calendar's `fingerprint` is a
canonical 64-bit diagnostic, **not** a collision-proof identity.

### 4.1 Transient generation and deterministic replay

`generate` returns fixture data for display **and** a `replay` object:
`{kind: "generated", generator, sample_index, expected_engine_id}`. The
returned generator object contains **all effective option fields**, including
defaults. JSON fixtures for generated calendars are outbound
only. No input operation accepts an arbitrary saved calendar fixture list.

For `resolve`, `calendar: {"kind": "original"}` evaluates the supplied
original snapshot. Alternatively, passing the generated replay object causes
the native CLI to create the generator, draw through the requested ordinal,
validate the selected transient calendar and resolve it **in the same process**.
A changed `engine.id` yields `REPLAY_MISMATCH` rather than silently producing
a different schedule. This is reproducible under the **same native engine ID,
source snapshot and effective options**, not across arbitrary native upgrades,
OS platforms or compiler configurations. The original snapshot remains
immutable and owns the round-to-Serie-A-day mapping.

Resolution `scope` is `FULL_SEASON` or `FINALIZED_PREFIX`. Full season requires
all scores `FINAL` and otherwise returns `PENDING_SCORES`; prefix uses only the
maximal contiguous finalized prefix and is marked `provisional: true` unless
all rounds were finalized. The first pending round truncates the prefix even
if later rounds happen to contain FINAL cells. Ranking defaults to `heapsort`;
`avl` is optional, with exactly identical sporting rank/result semantics.
Sporting ties share ranks (`1,1,3`); UUID order is presentation-only.

Result arrays are ordered by (sporting rank, UUID) for standings, by round
index then (home UUID, away UUID) for fixtures, and by UUID for per-team
statistics. `round_index` is **0-based**, `serie_a_matchday` remains the
**positive original day mapping**, and team references in JSON always use
UUIDs rather than ephemeral C indices.

### 4.2 Simulation and basic/advanced separation

`simulate` uses [`monte_carlo_run`](../../native/include/monte_carlo.h) on one
snapshot and one configured generator. Every result includes a separately
resolved original-calendar `baseline`, progress-independent count fields,
`completion`, and explicit `provisional` status.

- **`basic`** is the *noob* UI view: baseline rank, observed first-place
  frequency, mean rank and mean championship points per team. Frequencies
  are **observed** and divide by `accepted_samples`, not requested samples.
- **`advanced`** is the *pro* view: per-team rank counts, observed variance
  and goal-difference means, aggregate generator diagnostics and exact
  uniqueness counts **only if uniqueness was tracked**. Without tracking,
  `unique_calendars` and `duplicate_attempts` are `null`.
- `statistics: "basic"` returns only `basic`; `"advanced"` only `advanced`;
  `"both"` returns both namespaces. The response echoes `statistics_view`;
the schema enforces which result namespaces may be present. The baseline is
included in every case.

When no sample is accepted, means and frequencies are JSON `null`, never NaN
or zero invented as an estimate. The result's `completion` distinguishes
`completed`, `attempt_limit`, `cancelled` and `uniqueness_memory_limit`.
A completed subprocess can therefore produce a valid partial statistical
result. Completion does **not** imply the sampler is uniform, draws are
independent, or confidence intervals are available. Chain draws may be
autocorrelated. The native CLI **does not** promise bootstrap intervals or
Kendall tau-b unless a later native implementation explicitly adds them.

To enable detailed research observations, the trusted Python process may
supply a CLI argument `--observations-file PATH` **only for `simulate`**.
The JSON request has no arbitrary output path property. This CSV uses an
exclusively created staging file beside the requested destination; after the
final success response has been constructed and serialized, a rename
publishes the file before stdout is written. Rename failure produces `IO_ERROR`
instead of a success response. Invalid requests and unsuccessful simulations
remove their staging files. A hard crash or forced kill may leave a staged
file for the trusted Python worker to clean up. As with all subprocess
interfaces, a subsequent stdout I/O failure cannot retroactively undo an
already committed file. Application code must use
trusted paths and an isolated working directory; do not execute arbitrary
user-submitted path strings. The `observations_file` response flag indicates
whether a file was successfully finalized; it does not itself contain CSV.

## 5. Progress, cancellation and limits

If `emit_progress` is true, stderr carries newline-delimited progress records
matching the [progress schema](../../schemas/leaderboard-cli-v1-progress.schema.json).
They contain `request_id`, `attempts` and `accepted_samples`, with counts
monotonic within one invocation. Other diagnostic prose must not pollute the
progress stream when structured progress is enabled. stdout remains reserved
for the final response. `progress_every_attempts` determines callback
frequency; its default comes from `monte_carlo_options_default()` (1,000
attempts). The CLI also emits one terminal progress record when requested,
avoiding duplicate records if the final callback had the same counts.

On Linux, SIGINT and SIGTERM set a `volatile sig_atomic_t` cancellation flag.
The native progress callback observes the flag and requests cooperative
cancellation; handlers do not allocate, serialize or call non-async-signal-safe
functions. Cancellation latency is bounded by **callback opportunities**, not
by a promised wall-clock deadline. Python must still enforce an outer timeout
and may forcibly terminate a process if cooperative cancellation stalls.
A forcibly killed process may not produce any response; Python treats that as
a subprocess failure, not a valid zero-observation simulation.

Proposed **CLI v1** resource limits (separate from the broader C API):

| Resource | Bound |
| --- | ---: |
| Entire stdin request | 4 MiB, UTF-8 only |
| Entire stdout response | 8 MiB |
| Teams | 2-20, even |
| Imported rounds | 1-80 |
| Requested simulation samples | 1-10,000 |
| Total generation attempts | 1-100,000 |
| Generate/replay sample ordinal | 1-10,000 |
| Uniqueness tracking memory | 256 MiB |

When `track_uniqueness` is true and no memory budget is supplied, the CLI
uses a 64 MiB default (the explicit v1 maximum is 256 MiB). Enabling
`unique_only` requires `track_uniqueness`. Numeric output that cannot fit a
v1 diagnostic counter produces `LIMIT_EXCEEDED`, not out-of-schema JSON.

The CLI rejects data that violate configured budgets **before** allocating
large result matrices, and rejects output overflow rather than producing
truncated success JSON. Cross-field semantic restrictions such as exact
`team_count * round_count` scores and `attempts >= requested_samples` for a
completed unique-sampling run cannot be expressed fully by the schema.
The Python worker and container should apply tighter per-user job quotas,
resource cgroups and external timeouts as appropriate.

## 6. Errors and exit codes

Failures use `ok: false` with `error: {code, message, path}`. `path` is an
RFC 6901 JSON Pointer into the request when available; it is `null` for
malformed JSON or failures without a specific field. Messages are actionable,
bounded, and must not echo private snapshots, arbitrary filesystem paths or
secrets. The error is returned as one complete stdout JSON document with a
nonzero process exit code, except for OS-level interruption or hard crash.

| Exit code | Stable error codes | Meaning |
| --- | --- | --- |
| `0` | None | Valid complete or explicitly partial result |
| `2` | `INVALID_JSON`, `INVALID_SCHEMA_VERSION`, `INVALID_REQUEST`, `LIMIT_EXCEEDED` | Syntax, schema or user-side bounds |
| `3` | `INVALID_SNAPSHOT`, `UNSUPPORTED_CALENDAR`, `PENDING_SCORES`, `REPLAY_MISMATCH` | Native domain validation or replay conflict |
| `4` | `RESOURCE_EXHAUSTED` | Memory or other resource failure |
| `5` | `IO_ERROR` | Stream or output-file failure |
| `1` | `INTERNAL_ERROR` | Invariant violation or unexpected native failure |

Request `schema_version`, `request_id`, parsing and size limits must be checked
before constructing `competition_t`. The transport is failure-atomic for
application-visible results. Memory ownership remains C-local: borrowed
inputs outlive their use, native results are destroyed, and no C pointer
appears in JSON. Native negative errno is mapped to a stable external code,
not exposed as a platform-dependent value. Duplicate JSON keys, non-finite
numbers, malformed surrogate pairs and invalid UTF-8 fail parsing.

## 7. Fixtures, validation and acceptance

[Versioned fixtures](../../tests/fixtures/json_cli_v1/validate.request.json)
illustrate the request and response envelopes. Their simulated values,
fingerprints and `engine.id` are **illustrative**, not golden native results.
The test suite validates each example with `jsonschema` Draft 2020-12,
checks negative schema fixtures and deliberately confirms that a duplicate-team
snapshot passes *schema* validation and thus still needs *native* validation.

```sh
python -m pip install -r tests/requirements-json-contract.txt
python -m unittest discover -s tests/contracts -p test_json_contract.py -v
python scripts/check_docs.py
```

The linked-native `json_cli_native_contract` CTest exercises default scoring,
ranking backend equivalence, transient generation/replay, malformed inputs,
error classification, Monte Carlo response, progress, cancellation,
partial-completion and CSV publication. The independent installed-consumer
suite exercises the `crd-json` process boundary through Python `subprocess`.
The schema-only tests cannot replace those runtime checks. See the
[3.1.3 milestone](../milestones/3.1.3.md) and [local verification matrix](../development/local-verification.md).
