#!/usr/bin/env python3
"""Offline pre-publication checks for CRD source metadata and release assets.

Only standard-library modules are used; intended for Linux and Windows CI.
These checks validate archive contents and integrity, not binary signatures or
bit-for-bit reproducibility across toolchains.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import tarfile
import zipfile
from pathlib import Path, PurePosixPath

REPOSITORY = "AlessVolpe/CentralRankingDogma"
PROJECT_VERSION = re.compile(
    r"(?m)^project\(CentralRankingDogma VERSION (\d+\.\d+\.\d+) LANGUAGES C\)$"
)
PACKAGE_VERSION = re.compile(r'(?m)^set\(PACKAGE_VERSION "(\d+\.\d+\.\d+)"\)')


class ReleaseVerificationError(ValueError):
    """Release artifact or metadata does not meet the packaging contract."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReleaseVerificationError(message)


def read_version(repository: Path) -> str:
    content = (repository / "CMakeLists.txt").read_text(encoding="utf-8")
    match = PROJECT_VERSION.search(content)
    require(match is not None, "missing canonical CMake project version")
    return match.group(1)


def verify_source(repository: Path) -> str:
    """Catch mismatched active versions and stale canonical metadata."""
    version = read_version(repository)
    checks = {
        "README.md": f"**Current native version:** {version}",
        "docs/index.md": f"**{version} native C11 implementation**",
        "docs/development/packaging.md":
            f"find_package(CentralRankingDogma {version} CONFIG REQUIRED)",
        "examples/external_consumer/CMakeLists.txt":
            f"find_package(CentralRankingDogma {version} CONFIG REQUIRED)",
    }
    for filename, expected in checks.items():
        content = (repository / filename).read_text(encoding="utf-8")
        require(expected in content, f"{filename}: missing current version: {expected}")
    for name in ("request", "response", "progress"):
        filename = f"schemas/leaderboard-cli-v1-{name}.schema.json"
        schema = json.loads((repository / filename).read_text(encoding="utf-8"))
        canonical = (f"https://github.com/{REPOSITORY}/blob/master/"
                     f"{filename}")
        require(schema.get("$id") == canonical,
                f"{filename}: noncanonical schema $id")
        require("1.0" in (repository / filename).read_text(encoding="utf-8"),
                f"{filename}: JSON CLI v1 schema missing")
    require((repository / ".github/workflows/release.yml").is_file(),
            "automatic release workflow missing")
    require((repository / ".github/release.yml").is_file(),
            "generated release-notes categories missing")
    return version


def normalize_member(raw: str) -> str:
    """Strip tar's leading ./ while rejecting unsafe and ambiguous names."""
    require(raw != "" and "\\" not in raw and not raw.startswith("/"),
            f"unsafe archive path: {raw!r}")
    components = raw.split("/")
    require(".." not in components and not any(":" in p for p in components),
            f"unsafe archive path: {raw!r}")
    name = str(PurePosixPath(raw))
    return "" if name == "." else name


def verify_archive(path: Path, platform: str, version: str) -> None:
    """Inspect actual distribution bytes rather than just the CMake stage."""
    require(re.fullmatch(r"\d+\.\d+\.\d+", version) is not None,
            "archive version must be X.Y.Z")
    ext = "tar.gz" if platform == "linux" else "zip"
    expected_name = (f"central-ranking-dogma-v{version}-"
                     f"{'linux' if platform == 'linux' else 'windows-ucrt64'}-x86_64.{ext}")
    require(path.name == expected_name, f"incorrect archive filename: {path.name}")
    require(path.is_file() and path.stat().st_size > 0, "missing/empty archive")

    members: dict[str, int] = {}
    metadata: dict[str, bytes] = {}
    required = {
        "LICENSE", "README.md", "THIRD_PARTY_NOTICES.md",
        "include/central_ranking_dogma.h", "lib/libcentral_ranking_dogma.a",
        "lib/cmake/CentralRankingDogma/CentralRankingDogmaConfig.cmake",
        "lib/cmake/CentralRankingDogma/CentralRankingDogmaConfigVersion.cmake",
        "lib/cmake/CentralRankingDogma/CentralRankingDogmaTargets.cmake",
        "bin/crd-json" + (".exe" if platform == "windows-ucrt64" else ""),
    }
    version_path = "lib/cmake/CentralRankingDogma/CentralRankingDogmaConfigVersion.cmake"

    def track(name: str, size: int) -> None:
        require(name not in members, f"duplicate archive member: {name}")
        members[name] = size
        if name in required:
            require(size > 0, f"empty required release member: {name}")

    if platform == "linux":
        with tarfile.open(path, mode="r:gz") as archive:
            for member in archive:
                name = normalize_member(member.name)
                if not name:
                    continue
                require(member.isfile() or member.isdir(),
                        f"unsupported archive member type: {name}")
                if member.isdir():
                    continue
                track(name, member.size)
                if name == version_path:
                    stream = archive.extractfile(member)
                    require(stream is not None, "cannot read CMake package version")
                    metadata[name] = stream.read()
    else:
        with zipfile.ZipFile(path) as archive:
            require(archive.testzip() is None, "corrupt Windows ZIP entry")
            for info in archive.infolist():
                name = normalize_member(info.filename)
                if not name or info.is_dir():
                    continue
                # Reject UNIX-style symlinks inside ZIPs as well as odd paths.
                require((info.external_attr >> 16) & 0o170000 != 0o120000,
                        f"symlink not allowed in distribution: {name}")
                track(name, info.file_size)
                if name == version_path:
                    metadata[name] = archive.read(info)

    missing = sorted(required - members.keys())
    require(not missing, f"missing installed release files: {', '.join(missing)}")
    require(not any(n.startswith("lib/cmake/Leaderboard/") for n in members),
            "legacy Leaderboard CMake package is bundled")
    version_match = PACKAGE_VERSION.search(metadata[version_path].decode("utf-8"))
    require(version_match is not None and version_match.group(1) == version,
            "installed CMake package version does not match release archive")


def verify_checksums(directory: Path, version: str) -> None:
    """Verify all and only the two expected published archives."""
    expected = {
        f"central-ranking-dogma-v{version}-linux-x86_64.tar.gz",
        f"central-ranking-dogma-v{version}-windows-ucrt64-x86_64.zip",
    }
    file = directory / "SHA256SUMS.txt"
    require(file.is_file(), "missing SHA256SUMS.txt")
    observed: dict[str, str] = {}
    for line in file.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([a-f0-9]{64})\s+\*?([^/\\]+)", line)
        require(match is not None, f"malformed checksum line: {line!r}")
        hexdigest, name = match.groups()
        require(name not in observed, f"duplicate checksum entry: {name}")
        observed[name] = hexdigest
    require(set(observed) == expected, "checksum manifest does not match two release assets")
    for name, expected_digest in observed.items():
        payload = directory / name
        require(payload.is_file(), f"missing referenced artifact: {name}")
        with payload.open("rb") as handle:
            digest = hashlib.file_digest(handle, "sha256").hexdigest()
        require(digest == expected_digest, f"SHA-256 mismatch: {name}")
        verify_archive(payload, "linux" if name.endswith("tar.gz") else "windows-ucrt64", version)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    cmd = parser.add_subparsers(dest="mode", required=True)
    src = cmd.add_parser("source", help="check canonical repository metadata")
    src.add_argument("--repo", type=Path, default=Path("."))
    archive = cmd.add_parser("archive", help="validate one built distribution")
    archive.add_argument("--path", type=Path, required=True)
    archive.add_argument("--platform", choices=("linux", "windows-ucrt64"), required=True)
    archive.add_argument("--version", required=True)
    sums = cmd.add_parser("checksums", help="check published artifact integrity")
    sums.add_argument("--directory", type=Path, required=True)
    sums.add_argument("--version", required=True)
    args = parser.parse_args()
    try:
        if args.mode == "source":
            print(f"CRD release metadata verified: {verify_source(args.repo)}")
        elif args.mode == "archive":
            verify_archive(args.path, args.platform, args.version)
            print(f"CRD {args.platform} distribution verified: {args.path.name}")
        else:
            verify_checksums(args.directory, args.version)
            print(f"CRD {args.version} release archives and SHA-256 checksums verified")
    except (ReleaseVerificationError, OSError, ValueError, tarfile.TarError, zipfile.BadZipFile) as exc:
        parser.exit(1, f"Release verification failed: {exc}\n")


if __name__ == "__main__":
    main()
