# Failure safety: atomic operations and adversarial verification

## Abstract

The library uses opaque data structures and a private allocation boundary to validate failure atomicity, lifecycle contracts and malformed-input handling under reproducible tests.

## 1. Introduction and scientific motivation

A data structure is useful as a research instrument only if failures cannot silently corrupt subsequent results. This applies to container growth, tree rebalancing, competition construction and calendar generation.

## 2. Mathematical model

Let \(S\) be the externally observable logical state and \(f(S,x)\) an operation. For a failure-atomic operation, a reported failure leaves the prior state unchanged:

```math
f(S,x)=\mathrm{failure}\ \Longrightarrow\ S_{after}=S_{before}.
```

Equality concerns documented logical state, not pointer identity or transient internal allocation layout.

## 3. Algorithm and design

Allocation wrappers centralize fault injection for designated test builds. Constructors build partial private state and clean it on failure. Atomic two-team standings updates validate both increments before mutation. The calendar generator preserves externally visible RNG and chain state when a generation call fails.

## 4. Invariants and correctness

Every object has a single documented owner or borrowing relationship. Destroying partially built objects is safe where specified. Error returns must not disclose partially initialized handles to callers. Internal fault hooks are not installed with the production library.

## 5. Complexity

Failure checks and cleanup are bounded by the affected allocation/state sizes; adversarial failure sweeps multiply test workload by the number of injection sites. No universal constant-time rollback claim is made.

## 6. Public C11 interface and ownership

Public error and lifecycle contracts live in [`include/`](../../include/). The allocation failure controls are private in `src/internal/` and available only to dedicated test archives. Consumers cannot call those hooks in a production package.

## 7. Verification

The normal CTest suite exercises constructor failure matrices, invalid arguments, iterator invalidation, tree deletion, domain snapshot cleanup, standings overflow and calendar rollback. Optional libFuzzer targets cover the hashmap and BOSTree only; see [verification](../development/verification.md).

## 8. Limitations

Fault injection explores controlled allocation errors, not every possible operating-system failure, adversarial thread race or arbitrary memory corruption. Sanitizers support detection but do not prove absence of undefined behavior.

## References

The approach is documented historically in [3.0.7](../milestones/3.0.7.md) and [3.0.8](../milestones/3.0.8.md); the original [failure contract](../archive/failure-adversarial-contract.md) is preserved.
