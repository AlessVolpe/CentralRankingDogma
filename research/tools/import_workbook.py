#!/usr/bin/env python3
"""Import the existing 2026/27 regression TSV, without republishing team names.

The workbook has only a finalized prefix as of its snapshot date. Results are
provisional and must not be presented as final-season outcomes.
"""
from __future__ import annotations
import argparse
import csv
import tempfile
from pathlib import Path

from import_league import normalize


def convert(source: Path, output: Path, salt: bytes) -> dict:
    rows = []
    with source.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle, delimiter="\t"):
            if not row or row[0].startswith("#"):
                continue
            if len(row) != 7:
                raise ValueError("unexpected historical TSV row length")
            rounds, day, home, away, home_score, away_score, state = row
            if state not in {"F", "P"}:
                raise ValueError("unexpected historical score state")
            rows.append((int(rounds), int(day), home, away,
                         int(home_score), int(away_score), state))
    if not rows:
        raise ValueError("historical TSV contains no fixtures")
    teams = sorted({item for row in rows for item in (row[2], row[3])})
    with tempfile.TemporaryDirectory(prefix="fantacalcio-import-") as tmp:
        root = Path(tmp)
        with (root / "teams.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream); writer.writerow(("team_id", "name"))
            writer.writerows((name, name) for name in teams)
        with (root / "fixtures.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(("round", "matchday", "home_id", "away_id"))
            writer.writerows(row[:4] for row in rows)
        with (root / "scores.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(("matchday", "team_id", "points_tenths", "status"))
            for _, day, home, away, h, a, state in rows:
                writer.writerow((day, home, h, "FINAL" if state == "F" else "PENDING"))
                writer.writerow((day, away, a, "FINAL" if state == "F" else "PENDING"))
        return normalize(root, output, salt)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path,
                   default=Path("tests/fixtures/calendar_2026_2027.tsv"))
    p.add_argument("--output", type=Path,
                   default=Path("research/private/workbook_2026_2027"))
    p.add_argument("--salt-file", type=Path)
    args = p.parse_args()
    if args.salt_file:
        salt = args.salt_file.read_bytes()
        if len(salt) < 32:
            p.error("salt file requires at least 32 bytes")
    else:
        import secrets
        salt = secrets.token_bytes(32)
    print(convert(args.input, args.output, salt))


if __name__ == "__main__":
    main()
