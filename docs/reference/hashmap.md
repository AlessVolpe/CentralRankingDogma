# Hashmap: open addressing and typed callbacks

## Abstract

The native hashmap supports identifier lookup through open addressing with linear probing, backward-shift deletion and a portable C11 typed adapter. It is an infrastructure component, not a fantasy championship rule.

## 1. Introduction and scientific motivation

Hash tables trade probabilistic expected lookup cost for compact, contiguous storage. This implementation was historically adapted from David Leeds's implementation; source provenance belongs to [third-party notices](../../THIRD_PARTY_NOTICES.md), not to the scientific bibliography.

## 2. Mathematical model

For keys $x$ and table capacity $m$, $h(x)\in\{0,\ldots,m-1\}$. In linear probing the $j$-th candidate is

```math
p_j(x) = (h(x)+j)\bmod m,\qquad \alpha=\frac{n}{m}\le 0.75.
```

Here $n$ is the number of occupied entries. Clustering can degrade operation costs despite the load-factor bound.

## 3. Algorithm and design

Insertion scans from $h(x)$ to find an equal key or unused slot; growth may rehash existing entries. Lookup follows the same probe chain. Removal shifts subsequent elements backward where necessary to maintain reachability without tombstones. The modern `HASHMAP_TYPED_DEFINE` interface supplies type-correct callback adapters rather than incompatible pointer casts.

## 4. Invariants and correctness

- Every occupied key is reachable from its hashed start position without crossing an empty probe slot.
- No two occupied entries represent the same key under the supplied comparator.
- Backward-shift deletion preserves the reachability of surviving entries.
- Values may be borrowed while keys can be copied when ownership callbacks are configured; callers must inspect their selected lifecycle policy.

## 5. Complexity

Expected average lookup/update is $O(1)$ under adequate hashing and controlled load, but worst-case probing is $O(n)$. Rehashing is $O(n)$; insertion's typical amortized expectation is not a per-operation guarantee. Capacity-dependent memory use is $O(m)$.

## 6. Public C11 interface and ownership

Use [`hashmap_base.h`](../../native/include/hashmap_base.h) for the opaque base and [`hashmap_typed.h`](../../native/include/hashmap_typed.h) for named C11 typed wrappers. The deprecated `HASHMAP(...)` macros require GNU extensions. API clients must not inspect the private bucket representation. Key allocation callbacks, hash function and comparator determine storage and lifetime contracts.

<details>
<summary>Minimal typed usage pattern</summary>

```c
HASHMAP_TYPED_DEFINE(score_map, char, int)
/* score_map_init(&map, hashmap_hash_string, strcmp); */
/* score_map_put(&map, key, &score); */
/* score_map_cleanup(&map); */
```

</details>

## 7. Verification

Smoke, portable-callback, adversarial, differential and allocation-failure tests run within the normal CTest suite; see [verification](../development/verification.md). This does not imply a formal distributional guarantee for arbitrary user hash functions.

## 8. Limitations

Expected constant-time behavior assumes a reasonable hash distribution. Legacy GNU macros are not strict-C11 portable, and rehashing costs are workload-dependent.

## References

For background on searching, see [Knuth (1998)](../bibliography.md#knuth-1998). For the actual code adaptation, consult [third-party notices](../../THIRD_PARTY_NOTICES.md). Earlier contract material remains in the [archive](../archive/hashmap-contract.md).
