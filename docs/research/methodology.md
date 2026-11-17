# Research methodology: controlled calendar counterfactuals

## Research question

How sensitive are championship outcomes to the allocation of a fixed set of historical team performances to alternative **valid** fixture calendars? A separate research axis studies generator runtime and memory cost. This file is a **methodological plan**, not a completed empirical results section.

## Experimental controls

1. Import an immutable full calendar and team-by-Serie-A-day FINAL/PENDING score matrix through `competition_t`.
2. Validate the strict rematch schedule. Generate alternatives using seeded `calendar_t` with preserved directed encounters and fixed real-day mappings.
3. For full-season outcome experiments, require all scores FINAL. Automatic goal conversion and deterministic single-calendar match resolution are implemented in [3.1.0](../milestones/3.1.0.md). A finalized-prefix analysis is also possible; it must be labelled provisional.
4. Produce batches with separately recorded random seeds, independent or chain mode, algorithm settings, attempt budgets and software commit.
5. Evaluate single-calendar outcomes with 3.1.0 and descriptive distributions over batches with 3.1.1. Independent scientific replications and inferential analysis remain 3.1.2 scope. Do not infer uniform-space probabilities from fixture diagnostics or sampling frequencies alone.

## Metrics and reproducibility

Record league size $N$, rounds $R$, original fixture hash, seed, sampling mode, generation counts, modified base/season rounds, accepted exchanges, search exhaustion counts, unique fingerprints, runtime, compiler, flags and hardware. Since fingerprints are not collision-proof, never equate distinct hashes with a formal proof of distinct calendars.

The empirical measure $\hat{p}(A)=k_A/n$ is descriptive for observed sampled calendars. Without established mixing and uniformity, it should **not** be presented as the exact proportion of all feasible calendars satisfying event $A$.

## Distinguish questions

- **Correctness:** does every generated calendar satisfy the constraints? Validate independently.
- **Reproducibility:** does a fixed seed/configuration replay the same output? Check via deterministic tests.
- **Exploration:** how many different fixture assignments are visited? Measure fingerprints and structural distances.
- **Representativeness:** does the sampling mechanism explore its state space fairly? This remains an open scientific question; finite diversity tests are insufficient.
- **Performance:** what are time/memory costs on controlled workloads? See [performance](performance.md).

## Validity threats

A single league's historical team performances may not generalize to other seasons; score completeness matters; chain samples are autocorrelated; two-round moves may lack reachability; three-round backtracking is bounded; and fixed home/away counts remove some real-world variability. See [limitations](limitations.md) and [calendar sub-paper](../reference/calendar.md).

## References

Graph-model framing: [Januario et al. (2016)](../bibliography.md#januario-et-al-2016), [Ribeiro et al. (2023)](../bibliography.md#ribeiro-et-al-2023), and recoloring study [Urrutia et al. (2021)](../bibliography.md#urrutia-et-al-2021).
