"""Schema and example contract tests for planned 3.1.3 JSON CLI v1.

These fixtures are illustrative; they do not assert native algorithm outputs.
"""
from __future__ import annotations

import json
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
SCHEMAS = ROOT / "schemas"
FIXTURES = ROOT / "tests" / "fixtures" / "json_cli_v1"


class JsonCliV1ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.validators = {}
        for kind in ("request", "response", "progress"):
            path = SCHEMAS / f"leaderboard-cli-v1-{kind}.schema.json"
            data = json.loads(path.read_text(encoding="utf-8"))
            Draft202012Validator.check_schema(data)
            cls.validators[kind] = Draft202012Validator(data)

    def test_positive_fixtures(self) -> None:
        expected = {
            "validate.request.json": "request",
            "validate-pending.request.json": "request",
            "generate.request.json": "request",
            "generate-chain.request.json": "request",
            "resolve-original.request.json": "request",
            "resolve-prefix.request.json": "request",
            "resolve-generated.request.json": "request",
            "simulate.request.json": "request",
            "simulate-advanced.request.json": "request",
            "validate.response.json": "response",
            "generate.response.json": "response",
            "resolve.response.json": "response",
            "simulate.response.json": "response",
            "simulate-basic.response.json": "response",
            "simulate-advanced.response.json": "response",
            "error.response.json": "response",
            "progress.event.json": "progress",
        }
        for name, kind in expected.items():
            with self.subTest(fixture=name):
                data = json.loads((FIXTURES / name).read_text(encoding="utf-8"))
                self.validators[kind].validate(data)
        self.assertEqual(set(expected), {p.name for p in FIXTURES.glob("*.json")
                                         if not p.name.startswith(("invalid-", "semantic-"))})

    def test_invalid_fixtures(self) -> None:
        paths = sorted(FIXTURES.glob("invalid-*.json"))
        self.assertGreaterEqual(len(paths), 8)
        for path in paths:
            with self.subTest(fixture=path.name):
                kind = "request" if path.name.endswith(".request.json") else (
                    "response" if path.name.endswith(".response.json") else "progress")
                data = json.loads(path.read_text(encoding="utf-8"))
                self.assertFalse(self.validators[kind].is_valid(data))

    def test_semantic_rejection_is_separate(self) -> None:
        """JSON Schema cannot check duplicate UUIDs across snapshot records."""
        path = FIXTURES / "semantic-duplicate-team.request.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.validators["request"].validate(data)
        teams = data["competition"]["teams"]
        self.assertEqual(teams[0]["uuid"], teams[1]["uuid"])

    def test_response_operations_have_disjoint_success_shapes(self) -> None:
        requests = ["validate", "generate", "generate-chain", "resolve-original", "resolve-generated", "resolve-prefix", "simulate", "simulate-advanced"]
        for name in requests:
            with self.subTest(operation=name):
                data = json.loads((FIXTURES / f"{name}.request.json").read_text(encoding="utf-8"))
                self.assertTrue(self.validators["request"].is_valid(data))
        for name in ("validate", "generate", "resolve", "simulate"):
            with self.subTest(operation=name):
                data = json.loads((FIXTURES / f"{name}.response.json").read_text(encoding="utf-8"))
                data["operation"] = "simulate" if name != "simulate" else "resolve"
                self.assertFalse(self.validators["response"].is_valid(data))


if __name__ == "__main__":
    unittest.main()
