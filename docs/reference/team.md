# Team identity: UUIDs and nonnegative fixed-point totals

## Abstract

The C11 `team_t` represents a fantasy team, not an individual footballer. Its identity originates in the external application; scores are nonnegative integer tenths and are not standings points.

## 1. Introduction and scientific motivation

Human-readable names need not be unique or stable. UUID identity avoids conflating display labels with team keys. A competition snapshot converts external identifiers to compact zero-based indices for repeated numerical work.

## 2. Mathematical model

A team record is $t=(u,n,f)$ where $u\in\{0,1\}^{128}$ is a UUID, $n$ a bounded display name, and $f\in[0,2^{32}-1]$ an integer count of fantasy-point **tenths**. A 16-byte binary UUID is ordered lexicographically to assign deterministic indices within a competition.

## 3. Algorithm and design

`team_uuid_parse` accepts canonical 8-4-4-4-12 hexadecimal strings case-insensitively and does not require UUID version 4. `team_create` parses the identifier and copies the name. A competition calculates partial or final team sums from its imported score cells, rather than trusting a separate upstream total.

## 4. Invariants and correctness

Team UUIDs are unique within a validated competition, although names can repeat. A team record owns its `name`; competition accessors expose only borrowed read-only names and copied values. The fixed-point mapping avoids the loss associated with silently truncating binary floating-point inputs.

## 5. Complexity

UUID compare, parse and format have $O(1)$ cost for fixed-width identifiers. Team construction requires $O(|n|)$ time and storage to copy the bounded name. Constructing and sorting a competition's full team set is addressed in the [competition paper](competition.md).

## 6. Public C11 interface and ownership

See [`team.h`](../../include/team.h) for `team_uuid_parse`, `team_uuid_format`, `team_create` and `team_destroy`. A standalone team owns its copied name. The legacy `player_t` API was removed in 3.0.11; see the [migration milestone](../milestones/3.0.11.md).

## 7. Verification

UUID parsing, invalid names, ownership and fixed-point boundary conditions are covered by team and competition CTests. See [verification](../development/verification.md).

## 8. Limitations

UUID creation is external. The existing illustrative `uuid_generate` uses noncryptographic randomness and must not be treated as an application/database UUID service. Input decimal-to-tenths validation is the external adapter's responsibility.

## References

Module-level historical records: [3.0.11](../milestones/3.0.11.md) and [archived team-player model](../archive/team-player-model.md). For algorithmic pseudorandomness, see [Matsumoto and Nishimura (1998)](../bibliography.md#matsumoto-and-nishimura-1998).
