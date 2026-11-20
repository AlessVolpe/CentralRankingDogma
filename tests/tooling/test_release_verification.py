#!/usr/bin/env python3
"""Standard-library release metadata, archive and checksum negative tests."""
from __future__ import annotations

import hashlib
import importlib.util
import io
import json
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "scripts/verify_release_bundle.py"
spec = importlib.util.spec_from_file_location("verify_release_bundle", SOURCE)
assert spec and spec.loader
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)

VERSION = "3.1.4"
PACKAGE_VERSION = 'set(PACKAGE_VERSION "3.1.4")\n'


def members(platform: str) -> dict[str, bytes]:
    result = {
        "LICENSE": b"GPL-3.0-only\n",
        "README.md": b"Central Ranking Dogma\n",
        "THIRD_PARTY_NOTICES.md": b"Notices\n",
        "include/central_ranking_dogma.h": b"#ifndef CRD_H\n#define CRD_H\n#endif\n",
        "lib/libcentral_ranking_dogma.a": b"archive data",
        "lib/cmake/CentralRankingDogma/CentralRankingDogmaConfig.cmake": b"include(CentralRankingDogmaTargets.cmake)\n",
 "lib/cmake/CentralRankingDogma/CentralRankingDogmaConfigVersion.cmake": PACKAGE_VERSION.encode(),
        "lib/cmake/CentralRankingDogma/CentralRankingDogmaTargets.cmake": b"add_library(...)\n",
        "bin/crd-json.exe" if platform == "windows-ucrt64" else "bin/crd-json": b"executable",
    }
    return result


def bundle(directory: Path, platform: str, files: dict[str, bytes]) -> Path:
    suffix = "windows-ucrt64_x86_64.zip" if platform == "windows-ucrt64" else "linux_x86_64.tar.gz"
    # The distribution name uses a hyphen rather than an underscore.
    suffix = suffix.replace("_x86", "-x86")
    path = directory / f"central-ranking-dogma-v{VERSION}-{suffix}"
    if platform == "windows-ucrt64":
        with zipfile.ZipFile(path, "w") as archive:
            for name, payload in files.items():
                archive.writestr(name, payload)
    else:
        with tarfile.open(path, "w:gz") as archive:
            for name, payload in files.items():
                info = tarfile.TarInfo("./" + name)
                info.size = len(payload)
                archive.addfile(info, io.BytesIO(payload))
    return path


class ReleaseVerificationTests(unittest.TestCase):
    def test_valid_archives_and_checksums(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            paths = [bundle(directory, platform, members(platform))
                     for platform in ("linux", "windows-ucrt64")]
            for path, platform in zip(paths, ("linux", "windows-ucrt64")):
                verify.verify_archive(path, platform, VERSION)
            lines = [hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name
                     for path in paths]
            (directory / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n")
            verify.verify_checksums(directory, VERSION)
            paths[0].write_bytes(paths[0].read_bytes() + b"corruption")
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "SHA-256 mismatch"):
                verify.verify_checksums(directory, VERSION)

    def test_missing_public_header_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            content = members("linux")
            del content["include/central_ranking_dogma.h"]
            path = bundle(Path(tmp), "linux", content)
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "central_ranking_dogma.h"):
                verify.verify_archive(path, "linux", VERSION)

    def test_incorrect_installed_cmake_version_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            content = members("windows-ucrt64")
            content["lib/cmake/CentralRankingDogma/CentralRankingDogmaConfigVersion.cmake"] = \
                b'set(PACKAGE_VERSION "3.1.3")\n'
            path = bundle(Path(tmp), "windows-ucrt64", content)
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "package version"):
                verify.verify_archive(path, "windows-ucrt64", VERSION)

    def test_archive_traversal_and_duplicate_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            content = members("windows-ucrt64")
            content["../outside.txt"] = b"bad"
            path = bundle(Path(tmp), "windows-ucrt64", content)
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "unsafe archive path"):
                verify.verify_archive(path, "windows-ucrt64", VERSION)
            del content["../outside.txt"]
            path = bundle(Path(tmp), "windows-ucrt64", content)
            with zipfile.ZipFile(path, "a") as archive:
                archive.writestr("./README.md", b"duplicate")
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "duplicate archive member"):
                verify.verify_archive(path, "windows-ucrt64", VERSION)

    def test_source_metadata_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "CMakeLists.txt").write_text(
                "project(CentralRankingDogma VERSION 3.1.4 LANGUAGES C)\n")
            for path, value in {
                "README.md": "**Current native version:** 3.1.4",
                "docs/index.md": "**3.1.4 native C11 implementation**",
                "docs/development/packaging.md": "find_package(CentralRankingDogma 3.1.4 CONFIG REQUIRED)",
                "docs/development/migration-3.1.4.md": "find_package(CentralRankingDogma 3.1.4 CONFIG REQUIRED)",
                "examples/external_consumer/CMakeLists.txt": "find_package(CentralRankingDogma 3.1.4 CONFIG REQUIRED)",
                ".github/workflows/release.yml": "name: Release",
                ".github/release.yml": "changelog: {}",
            }.items():
                file = root / path
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(value, encoding="utf-8")
            for name in ("request", "response", "progress"):
                file = root / f"schemas/leaderboard-cli-v1-{name}.schema.json"
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(json.dumps({"$id":
                    f"https://github.com/AlessVolpe/CentralRankingDogma/blob/master/"
                    f"schemas/leaderboard-cli-v1-{name}.schema.json", "protocol": "1.0"}))
            self.assertEqual(verify.verify_source(root), VERSION)
            (root / "README.md").write_text("**Current native version:** 3.1.3")
            with self.assertRaisesRegex(verify.ReleaseVerificationError, "README.md"):
                verify.verify_source(root)


if __name__ == "__main__":
    unittest.main()
