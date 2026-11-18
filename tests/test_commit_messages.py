"""Offline regression tests for the scoped Conventional Commits policy."""
from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts.check_commit_messages import git_commits, load_policy, validate_subject


class CommitMessagePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.types, cls.scopes = load_policy()

    def assert_valid(self, subject: str) -> None:
        self.assertIsNone(validate_subject(subject, self.types, self.scopes), subject)

    def assert_invalid(self, subject: str) -> None:
        self.assertIsNotNone(validate_subject(subject, self.types, self.scopes), subject)

    def test_existing_and_current_scopes(self) -> None:
        for subject in (
            "feat(core): establish the native C11 library",
            "feat(calendar): preserve home-away orientations",
            "feat(domain): implement championship standings",
            "feat(simulation): implement Monte Carlo batch execution",
            "docs(readme): update mathematical notation",
            "docs(research): describe uncertainty limitations",
            "ci(actions): upgrade GitHub Actions dependencies",
            "ci(commits): enforce scoped Conventional Commits",
            "docs(contributing): document the subject policy",
            "build(cmake): export the public library target",
            "test(fuzzing): exercise the hashmap state machine",
            "perf(benchmarks): reduce benchmark allocation overhead",
            "feat(api)!: replace an incompatible C interface",
            "feat(integration): expose external snapshot contracts (#20)",
        ):
            with self.subTest(subject=subject):
                self.assert_valid(subject)

    def test_rejected_subjects(self) -> None:
        for subject in (
            "implement simulation",                       # no type or scope
            "feat: add a simulator",                     # missing scope
            "feat(): add a simulator",                   # empty scope
            "Feat(simulation): add a simulator",         # uppercase type
            "feat(Simulation): add a simulator",         # uppercase scope
            "feature(simulation): add a simulator",      # unknown type
            "feat(frontend): add a browser UI",          # Python app scope
            "feat(player): restore the removed player",  # deprecated scope
            "feat(simulation):",                         # missing description
            "feat(simulation): ",                        # blank description
            "feat(simulation): add test ",               # trailing spaces
            "Merge pull request #19 from feature",      # default merge commit
            "Merge branch 'master'",                    # generated merge commit
            "fix(match): trailing newline\nnot a subject",
        ):
            with self.subTest(subject=subject):
                self.assert_invalid(subject)

    def test_single_character_description_and_hyphenated_scope(self) -> None:
        self.assert_valid("fix(core): x")
        self.assert_invalid("fix(calendar-tools): x")

    def test_git_revision_range_does_not_lint_historical_commits(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            def git(*args: str) -> str:
                return subprocess.run(
                    ["git", *args], cwd=root, check=True, capture_output=True,
                    text=True, encoding="utf-8"
                ).stdout.strip()
            git("init", "-q")
            git("config", "user.email", "ci@example.invalid")
            git("config", "user.name", "CI Test")
            (root / "file.txt").write_text("first\n", encoding="utf-8")
            git("add", "file.txt")
            git("commit", "-q", "-m", "legacy unscoped commit")
            base = git("rev-parse", "HEAD")
            (root / "file.txt").write_text("second\n", encoding="utf-8")
            git("add", "file.txt")
            git("commit", "-q", "-m", "fix(core): preserve historical records")
            head = git("rev-parse", "HEAD")
            old_cwd = Path.cwd()
            try:
                import os
                os.chdir(root)
                commits = git_commits(f"{base}..{head}")
            finally:
                os.chdir(old_cwd)
            self.assertEqual(len(commits), 1)
            self.assertEqual(commits[0].subject,
                             "fix(core): preserve historical records")


if __name__ == "__main__":
    unittest.main()
