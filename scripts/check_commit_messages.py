#!/usr/bin/env python3
"""Validate scoped Conventional Commit subjects for this C repository.

Only selected new revisions are checked; the repository's pre-policy history
is intentionally not rewritten. The JSON policy is also used by unit tests.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import NamedTuple

POLICY_FILE = Path(__file__).with_name("commit_policy.json")
SUBJECT_PATTERN = re.compile(
    r"(?P<type>[a-z][a-z-]*)\((?P<scope>[a-z][a-z0-9-]*)\)"
    r"(?P<breaking>!)?: (?P<description>\S(?:.*\S)?)"
)


class Commit(NamedTuple):
    sha: str
    subject: str


def load_policy(path: Path = POLICY_FILE) -> tuple[frozenset[str], frozenset[str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    types = data["types"]
    scopes = data["scopes"]
    if (not isinstance(types, list) or not types or
            not isinstance(scopes, list) or not scopes or
            any(not isinstance(item, str) or not re.fullmatch(r"[a-z][a-z-]*", item)
                for item in types) or
            any(not isinstance(item, str) or not re.fullmatch(r"[a-z][a-z0-9-]*", item)
                for item in scopes) or
            len(types) != len(set(types)) or len(scopes) != len(set(scopes))):
        raise ValueError("invalid or duplicated commit policy entries")
    return frozenset(types), frozenset(scopes)


def validate_subject(subject: str, types: frozenset[str],
                     scopes: frozenset[str]) -> str | None:
    """Return a useful error or None for a permitted Conventional Commit."""
    match = SUBJECT_PATTERN.fullmatch(subject)
    if match is None:
        return "expected type(scope): imperative description (or type(scope)!: ... )"
    if match["type"] not in types:
        return f"unknown type '{match['type']}' (allowed: {', '.join(sorted(types))})"
    if match["scope"] not in scopes:
        return f"unknown scope '{match['scope']}' (allowed: {', '.join(sorted(scopes))})"
    return None


def git_commits(revision: str) -> list[Commit]:
    """Read authored subjects, excluding genuine Git merge commits by topology."""
    if not revision or revision.startswith("-"):
        raise ValueError("invalid git revision")
    result = subprocess.run(
        ["git", "log", "--no-merges", "--reverse", "--format=%H%x09%s", revision],
        check=True, capture_output=True, text=True, encoding="utf-8",
    )
    commits: list[Commit] = []
    for line in result.stdout.splitlines():
        sha, tab, subject = line.partition("\t")
        if not tab or not re.fullmatch(r"[0-9a-f]{40,64}", sha):
            raise ValueError("unexpected git log format")
        commits.append(Commit(sha, subject))
    return commits


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--range", dest="revision_range", metavar="BASE..HEAD",
                        help="validate every new commit reachable in the given range")
    source.add_argument("--commit", metavar="SHA", help="validate one commit")
    parser.add_argument("--title", help="also validate a pull request title")
    args = parser.parse_args(argv)
    if args.revision_range is None and args.commit is None and args.title is None:
        parser.error("provide --title, --range or --commit")

    try:
        types, scopes = load_policy()
        commits = git_commits(args.revision_range or args.commit) if (
            args.revision_range or args.commit) else []
    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            subprocess.CalledProcessError) as exc:
        print(f"Commit policy validation failed: {exc}", file=sys.stderr)
        return 2

    if (args.revision_range or args.commit) and not commits:
        print("Commit policy validation failed: no commits found", file=sys.stderr)
        return 2
    records = ([Commit("PR title", args.title)] if args.title is not None else [])
    records.extend(commits)
    problems = 0
    for record in records:
        problem = validate_subject(record.subject, types, scopes)
        if problem:
            print(f"FAIL {record.sha}: {record.subject!r}\n     {problem}")
            problems += 1
        else:
            print(f"PASS {record.sha}: {record.subject}")
    if problems:
        print(f"{problems} subject(s) violate scoped commit policy.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
