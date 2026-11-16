# Scientific data flow and validity gates

```mermaid
flowchart TD
    INPUT["External teams, complete fixtures, day-score cells"] --> SNAP["3.0.11 competition_t / validated snapshot"]
    SNAP --> GENERATOR["3.0.12 calendar generator"]
    GENERATOR --> VALID["Validated alternative calendars"]
    SNAP -.->|FINAL scores only| RESOLVE["3.1.0 resolver (planned)"]
    VALID -.-> RESOLVE
    RESOLVE -.-> STAND["standings_t / 3-1-0 totals"]
    STAND -.-> MONTE["3.1.1 Monte Carlo batches (planned)"]
    MONTE -.-> EVAL["3.1.2 scientific evaluation (planned)"]
```

**Solid arrows:** implemented import/fixture generation. **Dashed arrows:** intended downstream use, some components not yet implemented. In particular, `standings_t` **is** implemented, but the match resolver that supplies it automatic outcomes is not.

A structurally valid competition can contain `PENDING` scores. Calendar generation accepts this input. A full-season match simulation must check `competition_is_simulation_ready` before using scores. The [research methodology](../research/methodology.md) describes evaluation without inventing missing results.
