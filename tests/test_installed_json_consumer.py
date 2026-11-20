#!/usr/bin/env python3
"""PR D integration checks against an installed (not build-tree) executable."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "examples" / "python_subprocess_consumer"))
from leaderboard_consumer import invoke, LeaderboardResponseError, LeaderboardTransportError

BINARY = os.environ.get("LEADERBOARD_INSTALLED_JSON_BINARY")
FIXTURES = ROOT / "tests" / "fixtures" / "json_cli_v1"


@unittest.skipUnless(BINARY and Path(BINARY).is_file(), "requires installed leaderboard-json")
class InstalledConsumerTests(unittest.TestCase):
    def fixture(self, name):
        return json.loads((FIXTURES / name).read_text(encoding="utf-8"))

    def test_version(self):
        cp = subprocess.run([BINARY, "--version"], input=b"", capture_output=True,
                            timeout=10, check=True)
        self.assertEqual(cp.stderr, b"")
        self.assertEqual(cp.stdout, b"leaderboard-json 3.1.3\n")

    def test_validate(self):
        request = self.fixture("validate.request.json")
        response, events = invoke(BINARY, request)
        self.assertTrue(response["ok"])
        self.assertEqual(response["result"]["team_count"], 2)
        self.assertEqual(events, [])

    def test_generate_and_replay(self):
        request = self.fixture("generate.request.json")
        response, _ = invoke(BINARY, request)
        self.assertEqual(response["result"]["calendar"]["kind"], "generated")
        replay = response["result"]["replay"]
        resolution = self.fixture("resolve-original.request.json")
        resolution["calendar"] = replay
        resolved, _ = invoke(BINARY, resolution)
        self.assertEqual(resolved["result"]["calendar_kind"], "generated")

    def test_simulate_progress_and_csv(self):
        request = self.fixture("simulate.request.json")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "observations.csv"
            response, events = invoke(BINARY, request, observations_file=path)
            self.assertEqual(response["result"]["completion"], "completed")
            self.assertTrue(events)
            self.assertEqual(events[-1]["accepted_samples"], 3)
            self.assertTrue(path.is_file())
            self.assertIn("team_uuid", path.read_text(encoding="utf-8").splitlines()[0])
            self.assertFalse(list(Path(tmp).glob("*.lb-partial-*")))

    def test_error_envelope(self):
        req = self.fixture("validate.request.json")
        req["schema_version"] = "invalid"
        with self.assertRaises(LeaderboardResponseError) as caught:
            invoke(BINARY, req)
        self.assertEqual(caught.exception.code, "INVALID_SCHEMA_VERSION")
        self.assertEqual(caught.exception.exit_code, 2)

    def test_timeout(self):
        # A deliberately slow independent child checks subprocess timeout and
        # reaping, without timing assumptions about the fast native C engine.
        with tempfile.TemporaryDirectory() as tmp:
            sleeper = Path(tmp) / "slow.py"
            sleeper.write_text("#!/usr/bin/env python3\nimport time\ntime.sleep(10)\n", encoding="utf-8")
            sleeper.chmod(0o755)
            with self.assertRaisesRegex(LeaderboardTransportError, "timed out"):
                invoke(sleeper, self.fixture("validate.request.json"),
                       timeout=0.1, observations_file=None)


if __name__ == "__main__":
    unittest.main()
