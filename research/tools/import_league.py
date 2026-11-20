#!/usr/bin/env python3
"""Normalize an authorized league export into private, pseudonymized TSV.

Input CSVs:
  teams.csv: team_id,name
  fixtures.csv: round,matchday,home_id,away_id  (rounds 1-based)
  scores.csv: matchday,team_id,points_tenths,status  (FINAL or PENDING)

This tool does NOT scrape websites, establish redistribution permission, or
promise irreversible anonymization. Source exports and pseudonymized results
remain git-ignored by default. Publishing requires a separate rights review.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import hmac
import json
import secrets
import uuid
from pathlib import Path


def read_csv(path: Path, columns: set[str]) -> list[dict]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or not columns <= set(reader.fieldnames):
            raise ValueError(f"{path}: missing columns {columns}")
        rows = list(reader)
    if not rows:
        raise ValueError(f"empty dataset: {path}")
    return rows


def normalize(source: Path, output: Path, salt: bytes) -> dict:
    teams = read_csv(source / "teams.csv", {"team_id", "name"})
    fixtures = read_csv(source / "fixtures.csv", {"round", "matchday", "home_id", "away_id"})
    scores = read_csv(source / "scores.csv", {"matchday", "team_id", "points_tenths", "status"})
    original = [str(row["team_id"]) for row in teams]
    if len(original) != len(set(original)) or len(original) < 2 or len(original) % 2:
        raise ValueError("team IDs must be unique and team count even, >= 2")
    # Random per-import HMAC prevents trivial table inversion; it is not
    # proof against linkage through public matchday/fixture observations.
    mapping = {}
    for raw in original:
        digest = hmac.new(salt, raw.encode("utf-8"), hashlib.sha256).digest()
        mapping[raw] = str(uuid.UUID(bytes=digest[:16], version=4))
    if len(set(mapping.values())) != len(mapping):
        raise ValueError("unexpected pseudonym collision")
    aliases = {raw: f"Team-{i + 1:03d}"
               for i, raw in enumerate(sorted(original, key=lambda value: mapping[value]))}
    rounds: dict[int, tuple[int, list[tuple[str, str]]]] = {}
    for row in fixtures:
        index = int(row["round"])
        day = int(row["matchday"])
        if index <= 0 or day <= 0:
            raise ValueError("rounds and matchdays must be positive")
        home, away = str(row["home_id"]), str(row["away_id"])
        if home not in mapping or away not in mapping or home == away:
            raise ValueError("fixture references nonexistent/repeated team")
        if index not in rounds:
            rounds[index] = (day, [])
        if rounds[index][0] != day:
            raise ValueError("inconsistent matchday mapping")
        rounds[index][1].append((home, away))
    if sorted(rounds) != list(range(1, len(rounds) + 1)):
        raise ValueError("round indices must be 1..R with no gaps")
    days = [rounds[index][0] for index in sorted(rounds)]
    if days != sorted(set(days)):
        raise ValueError("matchdays must be strictly increasing")
    for day, match_list in rounds.values():
        if len(match_list) != len(teams) // 2:
            raise ValueError(f"incomplete round for matchday {day}")
        occurrences = [s for pair in match_list for s in pair]
        if sorted(occurrences) != sorted(original):
            raise ValueError(f"a round does not play each team exactly once: day {day}")
    seen = {}
    for row in scores:
        day, team = int(row["matchday"]), str(row["team_id"])
        status = str(row["status"]).strip().upper()
        value = int(row["points_tenths"])
        if day not in days or team not in mapping or (day, team) in seen:
            raise ValueError("unexpected/duplicate score entry")
        if status not in ("FINAL", "PENDING") or value < 0 or value > 0xffffffff:
            raise ValueError("invalid score/status")
        if status == "PENDING" and value != 0:
            raise ValueError("pending scores must be zero")
        seen[day, team] = (value, status)
    if len(seen) != len(teams) * len(rounds):
        raise ValueError("missing score entries")
    lines = [f"TEAM\t{mapping[raw]}\t{aliases[raw]}" for raw in sorted(original, key=lambda value: mapping[value])]
    for index in sorted(rounds):
        day, match_list = rounds[index]
        for home, away in match_list:
            lines.append(f"FIXTURE\t{index - 1}\t{day}\t{mapping[home]}\t{mapping[away]}")
        for team in sorted(original, key=lambda value: mapping[value]):
            value, status = seen[day, team]
            lines.append(f"SCORE\t{index - 1}\t{mapping[team]}\t{value}\t{status}")
    output.mkdir(parents=True, exist_ok=True)
    target = output / "competition.tsv"
    target.write_text("\n".join(lines) + "\n", encoding="ascii")
    manifest = {"format": "leaderboardsystem-research-tsv-v1", "team_count": len(teams),
                "round_count": len(rounds), "final_scores": sum(x[1] == "FINAL" for x in seen.values()),
                "source": "authorized local export; redistribution status NOT established",
                "privacy": "pseudonymized; linkage may remain possible",
                "competition_sha256": hashlib.sha256(target.read_bytes()).hexdigest()}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source", type=Path, help="directory containing three original CSV exports")
    p.add_argument("--output", type=Path, default=Path("research/private/normalized"))
    p.add_argument("--salt-file", type=Path, help="optional existing private 32-byte HMAC key")
    args = p.parse_args()
    if args.salt_file:
        salt = args.salt_file.read_bytes()
        if len(salt) < 32:
            p.error("salt file requires at least 32 bytes")
    else:
        salt = secrets.token_bytes(32)
    print(json.dumps(normalize(args.source, args.output, salt), indent=2))


if __name__ == "__main__":
    main()
