#!/usr/bin/env python3
"""3.1.3 PR B/C black-box JSON CLI tests; no private C headers required."""
from __future__ import annotations

import copy
import json
import os
import signal
import tempfile
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures" / "json_cli_v1"
BINARY = os.getenv("CRD_JSON_BINARY")
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None
if Draft202012Validator is not None:
    RESPONSE_SCHEMA = Draft202012Validator(json.loads(
        (ROOT / "schemas" / "leaderboard-cli-v1-response.schema.json").read_text(encoding="utf-8")))
else:
    RESPONSE_SCHEMA = None


@unittest.skipUnless(BINARY and Path(BINARY).exists(), "set CRD_JSON_BINARY")
class NativeCliContract(unittest.TestCase):
    def call(self, request: dict | bytes, args: list[str] | None = None) -> tuple[int, dict, str]:
        raw = request if isinstance(request, bytes) else json.dumps(request).encode("utf-8")
        completed = subprocess.run([BINARY, *(args or [])], input=raw,
                                   capture_output=True, timeout=30)
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
        rc, obj, events = self.call(self.fixture("simulate.request.json"))
        self.assertEqual(rc, 0, obj)
        self.assertTrue(obj["ok"])
        self.assertEqual(obj["operation"], "simulate")
        self.assertTrue(events)


    def test_simulation_basic_advanced_and_baseline(self):
        request = self.fixture("simulate.request.json")
        rc, result, events = self.call(request)
        self.assertEqual(rc, 0, (result, events))
        result = result["result"]
        self.assertEqual((result["completion"], result["requested_samples"],
                          result["attempts"], result["accepted_samples"]),
                         ("completed", 3, 3, 3))
        self.assertFalse(result["provisional"])
        self.assertEqual(result["statistics_view"], "both")
        self.assertEqual(len(result["baseline"]), 2)
        for team in result["basic"]["teams"]:
            self.assertEqual(team["baseline_rank"], team["mean_sporting_rank"])
            self.assertIn(team["first_place_frequency"], (0, 1))
        advanced = result["advanced"]
        self.assertEqual(advanced["uniqueness"], {
            "tracked": True, "unique_calendars": 1, "duplicate_attempts": 2})
        for team in advanced["teams"]:
            self.assertEqual(team["observations"], 3)
            self.assertEqual(sum(entry["count"] for entry in team["rank_frequencies"]), 3)
        observed = [json.loads(line) for line in events.splitlines()]
        self.assertTrue(observed)
        self.assertEqual(observed[-1]["attempts"], 3)
        self.assertEqual(observed[-1]["accepted_samples"], 3)
        for event in observed:
            self.assertEqual(set(event), {"schema_version", "request_id", "event",
                                          "attempts", "accepted_samples"})
            self.assertEqual(event["event"], "progress")

    def test_simulation_views_and_backend_equivalence(self):
        request = self.fixture("simulate.request.json")
        request["emit_progress"] = False
        request["track_uniqueness"] = False
        request.pop("uniqueness_memory_limit_bytes")
        rc, heap, err = self.call(request)
        self.assertEqual(rc, 0, heap)
        self.assertEqual(err, "")
        self.assertEqual(heap["result"]["advanced"]["uniqueness"], {
            "tracked": False, "unique_calendars": None, "duplicate_attempts": None})
        request["ranking_backend"] = "avl"
        rc, avl, _ = self.call(request)
        self.assertEqual(rc, 0, avl)
        self.assertEqual(avl["result"], heap["result"])
        request["statistics"] = "basic"
        rc, basic, _ = self.call(request)
        self.assertEqual(rc, 0, basic)
        self.assertIn("basic", basic["result"])
        self.assertNotIn("advanced", basic["result"])
        request["statistics"] = "advanced"
        rc, advanced, _ = self.call(request)
        self.assertEqual(rc, 0, advanced)
        self.assertNotIn("basic", advanced["result"])
        self.assertIn("advanced", advanced["result"])

    def test_simulation_budget_and_semantic_failures(self):
        request = self.fixture("simulate.request.json")
        request["requested_samples"] = 3
        request["max_generation_attempts"] = 4
        request["unique_only"] = True
        request["emit_progress"] = False
        rc, result, _ = self.call(request)
        self.assertEqual(rc, 0, result)
        self.assertEqual((result["result"]["completion"], result["result"]["accepted_samples"]),
                         ("attempt_limit", 1))
        request["track_uniqueness"] = False
        request.pop("uniqueness_memory_limit_bytes")
        rc, invalid, _ = self.call(request)
        self.assertEqual((rc, invalid["error"]["code"]), (2, "INVALID_REQUEST"))
        request = self.fixture("simulate.request.json")
        request["scope"] = "FULL_SEASON"
        request["competition"] = self.fixture("validate-pending.request.json")["competition"]
        rc, invalid, _ = self.call(request)
        self.assertEqual((rc, invalid["error"]["code"]), (3, "PENDING_SCORES"))
        request["scope"] = "FINALIZED_PREFIX"
        rc, provisional, _ = self.call(request)
        self.assertEqual(rc, 0, provisional)
        self.assertTrue(provisional["result"]["provisional"])
        self.assertEqual(provisional["result"]["evaluated_rounds"], 1)
        for score in request["competition"]["scores"]:
            if score["serie_a_matchday"] == 1:
                score["points_tenths"] = 0
                score["status"] = "PENDING"
        rc, invalid, _ = self.call(request)
        self.assertEqual((rc, invalid["error"]["code"]), (3, "PENDING_SCORES"))

    def test_simulation_zero_observations_are_null(self):
        request = self.fixture("simulate.request.json")
        request["unique_only"] = True
        request["track_uniqueness"] = True
        request["uniqueness_memory_limit_bytes"] = 1
        request["emit_progress"] = False
        rc, result, _ = self.call(request)
        self.assertEqual(rc, 0, result)
        data = result["result"]
        self.assertEqual(data["completion"], "uniqueness_memory_limit")
        self.assertEqual(data["accepted_samples"], 0)
        for team in data["basic"]["teams"]:
            self.assertIsNone(team["first_place_frequency"])
            self.assertIsNone(team["mean_sporting_rank"])
            self.assertIsNone(team["mean_championship_points"])
        for team in data["advanced"]["teams"]:
            self.assertEqual(team["observations"], 0)
            self.assertIsNone(team["variance_championship_points"])
            self.assertIsNone(team["mean_goal_difference"])

    def test_simulation_observation_csv_publication_and_cleanup(self):
        with tempfile.TemporaryDirectory() as temp:
            destination = Path(temp) / "observations.csv"
            request = self.fixture("simulate.request.json")
            request["emit_progress"] = False
            rc, result, _ = self.call(request, ["--observations-file", str(destination)])
            self.assertEqual(rc, 0, result)
            self.assertTrue(result["result"]["observations_file"])
            self.assertTrue(destination.is_file())
            text = destination.read_text(encoding="utf-8")
            self.assertIn("team_uuid", text.splitlines()[0])
            self.assertEqual(len(text.splitlines()), 1 + 2 * 3)
            self.assertFalse(list(Path(temp).glob("*.lb-partial-*")))
            request["unique_only"] = True
            request["track_uniqueness"] = False
            destination2 = Path(temp) / "should-not-exist.csv"
            rc, invalid, _ = self.call(request, ["--observations-file", str(destination2)])
            self.assertEqual(rc, 2)
            self.assertEqual(invalid["error"]["code"], "INVALID_REQUEST")
            self.assertFalse(destination2.exists())
            self.assertFalse(list(Path(temp).glob("*.lb-partial-*")))
            rc, invalid, _ = self.call(self.fixture("validate.request.json"),
                                       ["--observations-file", str(destination2)])
            self.assertEqual((rc, invalid["error"]["code"]), (2, "INVALID_REQUEST"))

    @unittest.skipUnless(os.name == "posix", "POSIX cooperative signals")
    def test_simulation_signal_cancellation(self):
        request = self.fixture("simulate.request.json")
        request.update({"requested_samples": 10000, "max_generation_attempts": 10000,
                        "progress_every_attempts": 1, "emit_progress": True,
                        "track_uniqueness": False, "unique_only": False})
        request.pop("uniqueness_memory_limit_bytes")
        raw = json.dumps(request).encode("utf-8")
        with subprocess.Popen([BINARY], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
            assert proc.stdin is not None and proc.stderr is not None
            proc.stdin.write(raw)
            proc.stdin.close()
            # Read an actual progress event before signalling so SIGINT cannot
            # arrive before the native handler has been installed.
            first = proc.stderr.readline()
            self.assertTrue(first, "simulation produced no progress before exiting")
            self.assertEqual(json.loads(first)["event"], "progress")
            if proc.poll() is None:
                proc.send_signal(signal.SIGINT)
            rest_err = proc.stderr.read()
            assert proc.stdout is not None
            stdout = proc.stdout.read()
            rc = proc.wait(timeout=20)
        result = json.loads(stdout)
        if RESPONSE_SCHEMA is not None:
            RESPONSE_SCHEMA.validate(result)
        self.assertEqual(rc, 0, (result,rest_err))
        self.assertEqual(result["result"]["completion"], "cancelled")
        self.assertLess(result["result"]["accepted_samples"], 10000)
        self.assertTrue(rest_err or first)


if __name__ == "__main__":
    unittest.main()
