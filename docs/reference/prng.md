# MT19937: pseudorandomness and reproducibility

## Abstract

The library provides deterministic MT19937 streams with both modern reference-compatible seeding and a historical compatibility mode. Neither is a cryptographically secure random-number generator.

## 1. Introduction and scientific motivation

Reproducibility requires an explicit generator, seed and bounded random draw contract. MT19937 was developed by Matsumoto and Nishimura (1998); the native library retains the historical generator for compatibility but prefers the modern standard-seeded API.

## 2. Mathematical model

The MT19937 engine has a theoretical period of

```math
2^{19937}-1,
```

which is a property of its state recurrence, not evidence of uniformity over valid fixture calendars. Given a fixed implementation and seed $z$, the output sequence is deterministic: $X_k(z)$ is reproducible.

## 3. Algorithm and design

The implementation generates 32-bit words via the MT19937 state twist and tempering transformations. `mtwister_seed_standard` uses the published standard seeding recurrence; the historical `seedRand` uses a different recurrence and must not be treated as identical for the same seed. Bounded calendar draws reject overflow-region residues rather than using a biased raw modulo operation.

## 4. Invariants and correctness

A fixed standard seed produces the published MT19937 reference sequence for the supported target integer types. The legacy seed variant intentionally differs. A bounded rejection sampler, when supplied uniform underlying 32-bit outputs, avoids residue-frequency distortion in mapping to a range.

## 5. Complexity

A pseudorandom draw has amortized $O(1)$ time with a fixed 624-word state; initialization is $O(624)$, constant relative to league size. Rejection sampling has expected bounded work but not a deterministic fixed iteration count for every possible stream.

## 6. Public C11 interface and ownership

[`mtwister.h`](../../include/mtwister.h) exposes `mtwister_new`, `mtwister_seed_standard`, `mtwister_next_u32` and `mtwister_destroy`, plus historical `seedRand`/`genRandLong`. Heap-owned states must be destroyed; stack-allocated `mtwister_t` must not be passed to `mtwister_destroy`.

## 7. Verification

Published seed vectors and historical regression vectors are exercised in CTest. Calendar determinism tests verify seeds under the documented platform contract. See [verification](../development/verification.md).

## 8. Limitations

MT19937 is unsuitable for cryptographic UUIDs or unpredictable adversarial inputs. Deterministic streams do not establish independently sampled or uniformly distributed calendars; see [research limitations](../research/limitations.md).

## References

[Matsumoto and Nishimura (1998)](../bibliography.md#matsumoto-and-nishimura-1998); [Durstenfeld (1964)](../bibliography.md#durstenfeld-1964) for shuffling context. Historical detail is [archived](../archive/portability-prng-3.0.9.md).
