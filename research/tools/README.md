# Research command-line tools

Run the commands in this directory from the repository root so paths in the
published research protocol resolve consistently. Python dependencies are
declared in `research/requirements.txt`, not in the native CMake package.

| Script | Role |
| --- | --- |
| `run_experiments.py` | Run native benchmark and simulation experiments |
| `analyze.py` | Describe and compare saved experimental observations |
| `summarize_benchmarks.py` | Aggregate per-replication benchmark outputs |
| `memory_profile.py` | Collect separate process-wide Massif measurements |
| `import_league.py` | Import authorized historical league exports |
| `import_workbook.py` | Convert a normalized historical workbook fixture |
| `verify_evidence.py` | Audit manifests and recorded scientific evidence |

See `research/README.md` for invocation syntax and scientific limitations.
Published evidence under `research/results/3.1.2/` remains immutable.
