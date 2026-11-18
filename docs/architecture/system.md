# System boundary: native C library and future application

This is a **planned integration architecture**, not an assertion that the Python application exists today. See [current C architecture](internal-c.md) for the implemented native foundation.

```mermaid
flowchart LR
    UI["Future web client"] <--> PY["Future Python application"]
    PY <--> DB[(Future league database)]
    PY -.->|Future snapshot transfer| C["LeaderboardSystem / ISO C11"]
    C -.->|Future experiment results| PY
    C --> CLI["Current native example consumers"]
```

The eventual Python service will own league/team registration, external UUID generation, database persistence, importing the full original calendar and reporting analysis. The native library receives plain immutable input snapshots, not a database connection. The transport schema and bindings belong to [3.1.3](../milestones/3.1.3.md), **not 3.0.12**.

The generic data structures, Fantacalcio match/season resolver and Monte Carlo batch engine are implemented in the native C11 library. Planned native work is the 3.1.2 ranking-backend integration and scientific evaluation, followed by 3.1.3 external transfer contracts. The Python web client, league database and import workflow remain separate future work.
