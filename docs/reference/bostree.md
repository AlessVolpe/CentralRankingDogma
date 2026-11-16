# BOSTree: AVL order statistics, deletion and lifecycle

## Abstract

The BOSTree is a balanced binary search tree augmenting each node with subtree cardinality. It implements ordered lookup, insertion, deletion, rank and selection.

## 1. Introduction and scientific motivation

Rank and selection require structure beyond a basic binary-search tree. AVL balancing bounds tree height while maintained subtree counts support indexed navigation. The original code adaptation is credited in [third-party notices](../../THIRD_PARTY_NOTICES.md).

## 2. Mathematical model

For a node \(v\), let \(L(v)\) and \(R(v)\) denote its children. Subtree cardinality and balance satisfy

```math
S(v)=1+S(L(v))+S(R(v)),\qquad S(\varnothing)=0,
```

```math
|h(L(v))-h(R(v))|\le 1.
```

If \(k\) is the left subtree size, selecting index \(i\) descends left for \(i<k\), returns the node for \(i=k\), or descends right for \(i>k\) with adjusted index \(i-k-1\).

## 3. Algorithm and design

AVL rotations restore the height bound after updates and recompute subtree metadata. Deletion handles leaf, one-child and two-child cases; the two-child case transplants the in-order successor and rebalances affected ancestors. Lookup and traversal use the caller's comparator.

## 4. Invariants and correctness

The tree preserves BST ordering, parent links, AVL height balance, subtree cardinalities and valid successor/predecessor traversal. A rotation preserves in-order key sequence while updating local structural metadata. A retained weak reference is subject to the lifecycle rules after node removal and must not be dereferenced as live payload without checking validity.

## 5. Complexity

AVL height is \(O(\log n)\). Search, insertion, deletion, rank and selection are \(O(\log n)\) worst-case, assuming constant-cost comparisons. Storage is \(O(n)\) nodes; no claim is made about custom comparator cost.

## 6. Public C11 interface and ownership

Use opaque handles and accessors from [`bostree.h`](../../include/bostree.h), including `bostree_select`, `bostree_rank`, `bostree_lookup` and `bostree_node_is_valid`. The tree's finalization callback and retained-reference conventions control payload lifetime. Private node structure is not a public ABI.

## 7. Verification

CTest covers rotations, subtree size invariants, deletion orders, weak references, rank/select and randomized adversarial workloads. See [verification](../development/verification.md).

## 8. Limitations

The public API doesn't promise thread safety or ownership of arbitrary keys/data. Comparators must implement consistent ordering. Holding stale node pointers after deletion is unsafe except through the documented retained-reference contract.

## References

See [Knuth (1998)](../bibliography.md#knuth-1998) for search-tree background and [third-party notices](../../THIRD_PARTY_NOTICES.md) for implementation provenance. Earlier [AVL](../archive/bostree-contract.md) and [deletion](../archive/bostree-deletion-contract.md) contracts are archived.
