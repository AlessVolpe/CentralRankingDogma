# Scientific documentation and developer reference

This repository's GitHub Markdown is the canonical documentation source for the **3.1.0** C11 implementation. The connected Python application and documentation website are future work. The library is the research instrument; the documentation describes its mathematics, tested behavior, limitations and historical development.

## Research reading path

1. [Research objective and experimental method](research/methodology.md)
2. [Common notation and units](notation.md)
3. [Internal C architecture](architecture/internal-c.md) and [data flow](architecture/data-flow.md)
4. [Competition](reference/competition.md), [calendar](reference/calendar.md), [match scoring](reference/match.md), and [season resolution](reference/season-resolution.md) sub-papers
5. [Performance methodology](research/performance.md), [known limitations](research/limitations.md), and [bibliography](bibliography.md)
6. [Roadmap and implementation milestones](roadmap.md)

## Developer reading path

1. [Build instructions](development/building.md) and [installed consumers](development/packaging.md)
2. [System boundary](architecture/system.md), [internal C architecture](architecture/internal-c.md)
3. [API-level reference](#module-sub-papers) and [failure contracts](reference/failure-safety.md)
4. [Verification procedures](development/verification.md) and [contribution style](STYLE_GUIDE.md)

## Module sub-papers

| Generic infrastructure | Domain and simulation foundation |
| --- | --- |
| [Hashmap](reference/hashmap.md) | [Team identity](reference/team.md) |
| [BOSTree](reference/bostree.md) | [Competition snapshot](reference/competition.md) |
| [Ranking](reference/ranking.md) | [Calendar permutation](reference/calendar.md) |
| [MT19937](reference/prng.md) | [Championship standings](reference/standings.md) |
| [Failure safety](reference/failure-safety.md) | [Match scoring](reference/match.md) |
| | [Season resolution](reference/season-resolution.md) |

## Architecture

- [Internal native C design](architecture/internal-c.md), the primary architecture document
- [Future system and Python boundary](architecture/system.md)
- [Scientific data flow and temporal validity](architecture/data-flow.md)

## Development and source provenance

- [Canonical roadmap](roadmap.md) and [individual milestones](milestones/)
- [Documentation standards](STYLE_GUIDE.md) and [CI instructions](development/documentation.md)
- [Human-readable references](bibliography.md) and [central BibTeX](references.bib)
- [Unmodified historical snapshots](archive/README.md), retained to prevent silent loss of earlier scientific and implementation material

**Interpretation rule:** a proof establishes its stated assumptions; a test establishes its observed cases; neither implies statistical uniformity or general correctness beyond the documented contract. See [limitations](research/limitations.md).
