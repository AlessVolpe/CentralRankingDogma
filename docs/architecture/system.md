# System boundary: native C library and future application

This is a **planned integration architecture**, not an assertion that the Python application exists today. See [current C architecture](internal-c.md) for the implemented native foundation.

```mermaid
flowchart LR
    UI["Future web client"] <--> PY["Future Python application"]
    PY <--> DB[(Future league database)]
    PY -.->|Planned JSON stdin/stdout| TRANSPORT["3.1.3 crd-json CLI"]
    TRANSPORT -->|PR B native validate/generate/resolve calls| C["Central Ranking Dogma / ISO C11"]
    C --> CLI["Current native example consumers"]
```

The eventual Python service will own league/team registration, external UUID generation, database persistence, importing the full original calendar and reporting analysis. The native library receives plain immutable input snapshots, not a database connection. The transport schema and bindings belong to [3.1.3](../milestones/3.1.3.md), **not 3.0.12**.

The generic data structures, Fantacalcio match/season resolver, optional composite-key AVL ranking and Monte Carlo engine are implemented in the native C11 library. The 3.1.2 controlled scientific evaluation is complete within its documented experimental scope. The 3.1.3 [JSON CLI contract](../integration/json-cli-v1.md) is specified by merged PR A. PR B adds native snapshot validation, transient calendar generation and season resolution. Monte Carlo transport remains PR C, while the installed Python subprocess consumer and Linux container remain PR D. The web client, database and import workflow remain outside this repository.
