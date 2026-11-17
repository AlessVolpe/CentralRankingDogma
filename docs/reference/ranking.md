# Ranking: deterministic order over mutable signed scores

## Abstract

`ranking_t` couples a hashmap for identifier lookup with an order-statistics AVL tree for ordered scores. This is a domain-neutral ranking abstraction, distinct from Fantacalcio standings.

## 1. Introduction and scientific motivation

A ranking must support updates without rebuilding an entire sorted array and must decide ties unambiguously. Stable tie-breaking ensures deterministic output independently of insertion order.

## 2. Mathematical model

For records $a$ and $b$ with signed 64-bit scores $s$ and string identifiers $id$, define:

```math
a\prec b\iff(s_a>s_b)\lor (s_a=s_b\land id_a<id_b).
```

Zero-based rank is the number of strictly preceding records:

```math
\mathrm{rank}(a)=\sum_{b\ne a}\mathbf{1}[b\prec a].
```

## 3. Algorithm and design

A hashmap resolves IDs to records. The augmented AVL tree orders the score-ID pairs and supports selection by subtree cardinality. Updating an existing score removes and reinserts the ordered key when necessary while keeping the index and ordered structure consistent.

## 4. Invariants and correctness

Every stored ID has exactly one hashmap entry and one ordered tree entry. The score and ID tie-break agree between structures. Failed allocation paths must leave the published logical ranking unchanged.

## 5. Complexity

AVL order queries are $O(\log n)$ worst-case, after ID lookup. Hashmap lookup is expected $O(1)$, worst case $O(n)$; an upsert that changes ordering requires logarithmic tree operations plus possible hash reallocation.

## 6. Public C11 interface and ownership

Use [`ranking.h`](../../include/ranking.h): `ranking_new`, `ranking_upsert`, `ranking_get`, `ranking_at`, `ranking_remove` and `ranking_destroy`. Identifiers are copied by the ranking implementation. Signed 64-bit ranking scores are **not** implicitly championship points or fantasy scores.

## 7. Verification

Deterministic and differential reference-model tests cover rank, selection, ties, updates, removals, and injection failures. See [verification](../development/verification.md).

## 8. Limitations

This API does not encode sporting tie-break policies, fixture inputs or league rules. A specific sporting ranking must define its own ordering semantics.

## References

See [Knuth (1998)](../bibliography.md#knuth-1998) for searching and sorting background; the original milestone contract is [archived](../archive/ranking-contract.md).
