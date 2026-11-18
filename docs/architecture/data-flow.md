# Scientific data flow and validity gates

```mermaid
flowchart TD
    INPUT["External teams, complete fixtures, day-score cells"] --> SNAP["3.0.11 competition_t / validated snapshot"]
    SNAP --> GENERATOR["3.0.12 calendar generator"]
    GENERATOR --> VALID["Validated alternative calendars"]
    SNAP -->|FINAL scores only| RESOLVE["3.1.0 deterministic resolver"]
    VALID --> RESOLVE
    RESOLVE --> STAND["standings_t / 3-1-0 totals"]
    STAND --> MONTE["3.1.1 Monte Carlo batches"]
    MONTE -.-> EVAL["3.1.2 scientific evaluation (planned)"]
```

**Solid arrows:** implemented import, fixture generation, deterministic match/season resolution and Monte Carlo batch aggregation. **Dashed arrows:** scientific evaluation still planned for 3.1.2. The original calendar is also evaluated independently as a baseline without passing through the calendar generator.

A structurally valid competition can contain `PENDING` scores. Calendar generation accepts this input. The 3.1.0 resolver permits a fully FINAL season or the maximal consecutive FINAL round prefix; it never treats pending scores as zero. The [research methodology](../research/methodology.md) describes evaluation without inventing missing results.
