#!/usr/bin/env python3
"""3.1.3 PR B black-box JSON CLI tests; no private C headers required."""
from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures" / "json_cli_v1"
BINARY = os.getenv("LEADERBOARD_JSON_BINARY")
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None
if Draft202012Validator is not None:
    RESPONSE_SCHEMA = Draft202012Validator(json.loads(
        (ROOT / "schemas" / "leaderboard-cli-v1-response.schema.json").read_text(encoding="utf-8")))
else:
    RESPONSE_SCHEMA = None


@unittest.skipUnless(BINARY and Path(BINARY).exists(), "set LEADERBOARD_JSON_BINARY")
class NativeCliContract(unittest.TestCase):
    def call(self, request: dict | bytes) -> tuple[int, dict, str]:
        raw = request if isinstance(request, bytes) else json.dumps(request).encode("utf-8")
        completed = subprocess.run([BINARY], input=raw, capture_output=True, timeout=30)
        self.assertTrue(completed.stdout.endswith(b"\n"), completed.stderr)
        self.assertEqual(completed.stdout.count(b"\n"), 1, completed.stdout)
        self.assertLess(len(completed.stdout), 8 * 1024 * 1024 + 2)
        obj = json.loads(completed.stdout)
        self.assertEqual(obj["schema_version"], "1.0")
        self.assertIsInstance(obj["engine"]["id"], str)
        if RESPONSE_SCHEMA is not None:
            RESPONSE_SCHEMA.validate(obj)
        return completed.returncode, obj, completed.stderr.decode("utf-8", errors="replace")

    def fixture(self, name: str) -> dict:
        return json.loads((FIXTURES / name).read_text(encoding="utf-8"))

    def test_validate_and_pending(self):
        rc, obj, err = self.call(self.fixture("validate.request.json"))
        self.assertEqual(rc, 0, (obj, err))
        self.assertTrue(obj["ok"])
        self.assertEqual(obj["result"]["team_count"], 2)
        self.assertEqual(obj["result"]["finalized_prefix_rounds"], 1)
        self.assertTrue(obj["result"]["simulation_ready"])
        rc, obj, _ = self.call(self.fixture("validate-pending.request.json"))
        self.assertEqual(rc, 0, obj)
        self.assertFalse(obj["result"]["simulation_ready"])

    def test_invalid_input_categories(self):
        for name, code, status in [
            ("invalid-float-score.request.json", "INVALID_REQUEST", 2),
            ("invalid-unknown-key.request.json", "INVALID_REQUEST", 2),
            ("invalid-bad-status.request.json", "INVALID_REQUEST", 2),
            ("invalid-pending-nonzero.request.json", "INVALID_REQUEST", 2),
            ("semantic-duplicate-team.request.json", "INVALID_SNAPSHOT", 3),
        ]:
            with self.subTest(name=name):
                rc, obj, _ = self.call(self.fixture(name))
                self.assertEqual((rc, obj["ok"], obj["error"]["code"]),
                                 (status, False, code))
        for raw in [b'{"request_id":"x",', b'{"a":1,"a":2}',
                    b'{"request_id":"x", "invalid":"\xff"}',
                    b'{"request_id":"x","operation":"validate"} true',
                    b'{"request_id":"x","a":"\\ud800"}']:
            with self.subTest(raw=raw):
                rc, obj, _ = self.call(raw)
                self.assertEqual(rc, 2)
                self.assertEqual(obj["error"]["code"], "INVALID_JSON")
        req = self.fixture("validate.request.json")
        req["schema_version"] = "2.0"
        rc, obj, _ = self.call(req)
        self.assertEqual((rc, obj["error"]["code"]), (2, "INVALID_SCHEMA_VERSION"))

    def test_original_resolution_backend_equivalence(self):
        req = self.fixture("resolve-original.request.json")
        rc, heap, _ = self.call(req)
        self.assertEqual(rc, 0, heap)
        self.assertEqual(heap["result"]["evaluated_rounds"], 1)
        self.assertEqual(len(heap["result"]["fixtures"]), 1)
        self.assertEqual(len(heap["result"]["standings"]), 2)
        # Native default: 66.5 scores one goal, 59.0 scores zero.
        fixture = heap["result"]["fixtures"][0]
        self.assertEqual((fixture["home_fantasy_tenths"], fixture["away_fantasy_tenths"]),
                         (665, 590))
        self.assertEqual((fixture["home_goals"], fixture["away_goals"]), (1, 0))
        self.assertEqual(heap["result"]["standings"][0]["championship_points"], 3)
        req["ranking_backend"] = "avl"
        rc, avl, _ = self.call(req)
        self.assertEqual(rc, 0, avl)
        self.assertEqual(heap["result"], avl["result"])

    def test_custom_rules_and_validation(self):
        request = self.fixture("resolve-original.request.json")
        request["rules"] = {
            "goal_thresholds_tenths": [660, 700, 740, 780, 820, 860, 900, 940],
            "post_eighth_step_tenths": 40,
            "limit_victory_enabled": False,
            "limit_victory_max_gap_tenths": 0,
            "limit_victory_include_subthreshold": False,
            "limit_draw_enabled": False,
            "limit_draw_min_gap_tenths": 0,
            "limit_draw_include_subthreshold": False,
            "home_advantage_enabled": False,
            "home_advantage_tenths": 0,
            "home_advantage_exclude_final_cycle": False,
        }
        rc, result, _ = self.call(request)
        self.assertEqual(rc, 0, result)
        self.assertEqual(result["result"]["fixtures"][0]["home_goals"], 1)
        request["rules"]["unexpected"] = 1
        rc, result, _ = self.call(request)
        self.assertEqual((rc, result["error"]["code"]), (2, "INVALID_REQUEST"))

    def test_generated_replay_and_mismatch(self):
        request = self.fixture("generate.request.json")
        rc, result, _ = self.call(request)
        self.assertEqual(rc, 0, result)
        self.assertEqual(result["result"]["calendar"]["kind"], "generated")
        self.assertEqual(len(result["result"]["calendar"]["fingerprint"]), 16)
        selector = result["result"]["replay"]
        self.assertEqual(selector["expected_engine_id"], result["engine"]["id"])
        resolve = self.fixture("resolve-original.request.json")
        resolve["calendar"] = selector
        rc, resolved, _ = self.call(resolve)
        self.assertEqual(rc, 0, resolved)
        self.assertEqual(resolved["result"]["calendar_kind"], "generated")
        bad = copy.deepcopy(resolve)
        bad["calendar"]["expected_engine_id"] = "incompatible-build"
        rc, rejected, _ = self.call(bad)
        self.assertEqual((rc, rejected["error"]["code"]), (3, "REPLAY_MISMATCH"))

    def test_partial_and_no_longer_than_budget(self):
        req = self.fixture("resolve-prefix.request.json")
        rc, result, _ = self.call(req)
        self.assertEqual(rc, 0, result)
        self.assertEqual(result["result"]["scope"], "FINALIZED_PREFIX")
        req = self.fixture("resolve-original.request.json")
        req["competition"] = self.fixture("validate-pending.request.json")["competition"]
        rc, result, _ = self.call(req)
        self.assertEqual((rc, result["error"]["code"]), (3, "PENDING_SCORES"))
        req = self.fixture("generate.request.json")
        req["sample_index"] = 10001
        rc, result, _ = self.call(req)
        self.assertEqual((rc, result["error"]["code"]), (2, "INVALID_REQUEST"))

    def test_input_limit(self):
        rc, obj, _ = self.call(b"{" + b" " * (4 * 1024 * 1024) + b"}")
        self.assertEqual((rc, obj["error"]["code"]), (2, "LIMIT_EXCEEDED"))

    def test_subprocess_protocol(self):
        req = self.fixture("validate.request.json")
        first = self.call(req)
        second = self.call(req)
        self.assertEqual(first, second)
        rc, obj, _ = self.call(self.fixture("simulate.request.json"))
        self.assertEqual(rc, 2)
        self.assertEqual(obj["error"]["code"], "INVALID_REQUEST")


if __name__ == "__main__":
    unittest.main()
