# Mathematical notation and units

Notation is shared across the scientific sub-papers. Indices are **zero-based** internally. External Serie A matchdays are positive integers and are not required to start at 1.

| Symbol | Meaning |
| --- | --- |
| $N$ | Even number of fantasy teams, $N\ge 2$ |
| $R$ | Number of competition rounds, $R\ge 1$ |
| $V$ | Team-index set $\{0,\ldots,N-1\}$ |
| $M_r$ | Unordered perfect matching for league round $r$ |
| $P=N-1$ | Strict rematch period of a full single round robin |
| $K_N$ | Complete undirected graph on team set $V$ |
| $C(a,b)$ | Unordered season encounter multiplicity of teams $a,b$ |
| $D(a,b)$ | Directed encounters with $a$ home and $b$ away |
| $s_{ir}$ | Team $i$'s finalized fantasy score in integer tenths on competition round $r$'s Serie A day |
| $W,D,L$ | Wins, draws and losses in one standings experiment |
| $\mathcal{S}$ | Feasible calendar state space under the stated constraints |
| $T(n)$, $S(n)$ | Time and auxiliary-space complexity |

**Fixed point:** `uint32_t points_tenths` stores nonnegative fantasy scores exactly at one decimal place: `24835` means 2483.5. A pending score is different from a finalized zero. Fantasy totals are independent of calendar permutations; championship points are not.

**Vocabulary:** *round* means league round; *Serie A matchday* means real-world matchday; *fixture* is a directed home/away pairing; *calendar* is a mapping of fixtures to league rounds. This distinction is used consistently in the [competition](reference/competition.md) and [calendar](reference/calendar.md) papers.
