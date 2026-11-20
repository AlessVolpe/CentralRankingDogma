"""Independent, standard-library-only client for the installed leaderboard-json CLI.

This is an integration example, not the future Python service or a native binding.
"""
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any


class LeaderboardTransportError(RuntimeError):
    """The subprocess failed, timed out, or broke the wire protocol."""


class LeaderboardResponseError(LeaderboardTransportError):
    """The native engine returned a well-formed application error."""

    def __init__(self, code: str, message: str, exit_code: int):
        super().__init__(f"{code}: {message} (exit {exit_code})")
        self.code = code
        self.exit_code = exit_code


def invoke(binary: str | Path, request: dict[str, Any], *, timeout: float = 30.0,
           observations_file: str | Path | None = None) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    """Run exactly one request, returning the response and optional progress events.

    Raises LeaderboardResponseError for native error envelopes and
    LeaderboardTransportError for process/stream/protocol failures.
    """
    if timeout <= 0:
        raise ValueError("timeout must be positive")
    if not isinstance(request, dict):
        raise TypeError("request must be a JSON object")
    command = [str(binary)]
    if observations_file is not None:
        command.extend(["--observations-file", str(observations_file)])
    payload = (json.dumps(request, ensure_ascii=False, allow_nan=False,
                          separators=(",", ":")) + "\n").encode("utf-8")
    try:
        completed = subprocess.run(command, input=payload, capture_output=True,
                                   timeout=timeout, check=False)
    except subprocess.TimeoutExpired as exc:
        raise LeaderboardTransportError(f"leaderboard-json timed out after {timeout}s") from exc
    except OSError as exc:
        raise LeaderboardTransportError(f"cannot execute leaderboard-json: {exc}") from exc

    stdout = completed.stdout
    if not stdout.endswith(b"\n") or stdout.count(b"\n") != 1:
        raise LeaderboardTransportError("expected one newline-terminated JSON response")
    try:
        response = json.loads(stdout.decode("utf-8"))
    except (UnicodeError, ValueError) as exc:
        raise LeaderboardTransportError("invalid UTF-8 or JSON on stdout") from exc
    if not isinstance(response, dict) or response.get("schema_version") != "1.0":
        raise LeaderboardTransportError("unsupported or missing response schema_version")
    engine = response.get("engine")
    if not isinstance(engine, dict) or not isinstance(engine.get("id"), str) or not engine["id"]:
        raise LeaderboardTransportError("missing native engine identity")
    request_id = request.get("request_id")
    if response.get("request_id") != request_id:
        # Native parse failures are allowed a null request_id.
        if not (completed.returncode != 0 and response.get("request_id") is None):
            raise LeaderboardTransportError("response request_id does not match request")

    events = []
    try:
        for line in completed.stderr.splitlines():
            if not line:
                continue
            event = json.loads(line.decode("utf-8"))
            if not isinstance(event, dict) or event.get("event") != "progress" or \
                    event.get("schema_version") != "1.0":
                raise ValueError("unexpected progress event")
            events.append(event)
    except (UnicodeError, ValueError) as exc:
        raise LeaderboardTransportError("unexpected non-JSON data on stderr") from exc

    ok = response.get("ok")
    if ok is True and completed.returncode == 0:
        if response.get("operation") != request.get("operation") or "result" not in response:
            raise LeaderboardTransportError("success envelope missing operation or result")
        return response, events
    if ok is False and completed.returncode != 0:
        err = response.get("error")
        if isinstance(err, dict) and isinstance(err.get("code"), str) and \
                isinstance(err.get("message"), str):
            raise LeaderboardResponseError(err["code"], err["message"], completed.returncode)
    raise LeaderboardTransportError("process exit status disagrees with JSON envelope")


def main() -> int:
    parser = argparse.ArgumentParser(description="Consume an installed leaderboard-json binary")
    parser.add_argument("binary", type=Path, help="installed leaderboard-json path")
    parser.add_argument("request", type=Path, help="JSON request file")
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    request = json.loads(args.request.read_text(encoding="utf-8"))
    response, events = invoke(args.binary, request, timeout=args.timeout)
    print(json.dumps(response, indent=2, ensure_ascii=False))
    print(f"Progress events: {len(events)}", file=__import__("sys").stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except LeaderboardTransportError as exc:
        raise SystemExit(str(exc)) from exc
