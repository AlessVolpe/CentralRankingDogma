#!/usr/bin/env python3
"""Offline checks for canonical GitHub Markdown documentation (stdlib only).

Historical docs/archive/ preserve pre-migration content with rendering errata;
old links there are not interpreted as new-style navigation contracts.
"""
from __future__ import annotations

import re
import sys
import unicodedata
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
REF_DIR = DOCS / "reference"
REQUIRED_SECTIONS = [
    "Abstract", "1. Introduction and scientific motivation", "2. Mathematical model",
    "3. Algorithm and design", "4. Invariants and correctness", "5. Complexity",
    "6. Public C11 interface and ownership", "7. Verification", "8. Limitations",
    "References",
]
ERRORS: list[str] = []


def fail(where: str, problem: str) -> None:
    ERRORS.append(f"{where}: {problem}")


def docs_files() -> list[Path]:
    paths = [ROOT / "README.md", ROOT / "CONTRIBUTING.md"]
    paths += [p for p in DOCS.rglob("*.md") if "archive" not in p.relative_to(DOCS).parts]
    return sorted(paths)


def slug(text: str) -> str:
    # Approximate GitHub's GitHub-Flavored Markdown heading slug algorithm.
    text = re.sub(r"<[^>]+>", "", text).lower()
    text = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", text)
    text = "".join(c for c in text if c in " -_" or c.isalnum() or unicodedata.category(c).startswith("M"))
    return re.sub(r" +", "-", text.strip())


def headings(md: str) -> set[str]:
    observed: dict[str, int] = {}
    names: set[str] = set()
    for line in md.splitlines():
        if not re.match(r"^#{1,6}\s+", line):
            continue
        title = re.sub(r"^#{1,6}\s+", "", line).strip().rstrip("# ")
        base = slug(title)
        occurrence = observed.get(base, 0)
        observed[base] = occurrence + 1
        names.add(base if occurrence == 0 else f"{base}-{occurrence}")
    return names


def check_links(path: Path, content: str) -> None:
    # Ignore fenced source examples. HTML <details> is supported by GitHub.
    in_fence = False
    for lineno, line in enumerate(content.splitlines(), 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for match in re.finditer(r"(?<!!)\[[^\]]+\]\(([^)]+)\)", line):
            value = match.group(1).strip()
            if value.startswith(("https://", "http://", "mailto:", "data:")):
                continue
            target = value.split(" ", 1)[0].strip("<>")
            if not target:
                fail(str(path), f"line {lineno}: empty link")
                continue
            rel, _, fragment = target.partition("#")
            destination = (path.parent / unquote(rel)).resolve() if rel else path.resolve()
            if not destination.is_relative_to(ROOT.resolve()):
                fail(str(path), f"line {lineno}: link escapes repository: {value}")
            elif not destination.exists():
                fail(str(path), f"line {lineno}: missing target: {value}")
            elif fragment and destination.is_file() and destination.suffix == ".md":
                if unquote(fragment).lower() not in headings(destination.read_text(encoding="utf-8")):
                    fail(str(path), f"line {lineno}: missing anchor: {value}")


def check_fences(path: Path, content: str) -> None:
    blocks: list[str] = []
    open_language: str | None = None
    for line in content.splitlines():
        match = re.match(r"^\s*```([A-Za-z\d_-]*)\s*$", line)
        if not match:
            continue
        if open_language is None:
            open_language = match.group(1)
            if open_language in {"math", "mermaid"}:
                blocks.append(open_language)
        else:
            open_language = None
    if open_language is not None:
        fail(str(path), "unclosed fenced code block")
    if "mermaid" in blocks and not any(re.search(r"```mermaid\s*\n\s*(flowchart|graph)\s", content) for _ in [0]):
        fail(str(path), "Mermaid block does not begin with a flowchart/graph declaration")


def check_inline_math(path: Path, content: str) -> None:
    """Reject unsupported TeX math delimiters in prose on GitHub.

    Fenced code and inline code literals are examples, not math to render.
    Inline math must use $...$ (or GitHub's dollar-backtick alternative).
    """
    in_fence = False
    for lineno, line in enumerate(content.splitlines(), 1):
        if re.match(r"^\s*(```|~~~)", line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        prose = re.sub(r"`+[^`]*`+", "", line)
        if re.search(r"\\(?:\(|\)|\[|\])", prose):
            fail(str(path.relative_to(ROOT)),
                 f"line {lineno}: unsupported TeX-style inline math delimiter; use $...$ on GitHub")


def check_unsupported_math_macros(path: Path, content: str) -> None:
    """Avoid renderer-rejected macros, including in historical documents."""
    for lineno, line in enumerate(content.splitlines(), 1):
        if re.search(r"\\operatorname\b", line):
            fail(str(path.relative_to(ROOT)),
                 f"line {lineno}: unsupported \\operatorname macro; use \\mathrm{{...}}")


def check_references() -> None:
    bib = (DOCS / "references.bib").read_text(encoding="utf-8")
    keys = re.findall(r"(?m)^@\w+\{([\w-]+)\s*,", bib)
    if len(keys) != len(set(keys)):
        fail("references.bib", "duplicate BibTeX key")
    pretty = (DOCS / "bibliography.md").read_text(encoding="utf-8")
    described = re.findall(r"Citation key: `([\w-]+)`", pretty)
    if sorted(keys) != sorted(described):
        fail("bibliography", "citation keys disagree between BibTeX and readable bibliography")
    # Enforce balanced BibTeX braces, including nested title braces.
    depth = 0
    for char in bib:
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        if depth < 0:
            fail("references.bib", "unmatched closing brace")
            break
    if depth:
        fail("references.bib", f"unbalanced entry braces ({depth})")


def check_project_layout_and_version() -> None:
    """Detect stale active package metadata and broken PR C source layout."""
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(
        r"(?m)^project\(CentralRankingDogma\s+VERSION\s+(\d+\.\d+\.\d+)\b",
        cmake,
    )
    if not match:
        fail("CMakeLists.txt", "canonical project name/version missing")
        return
    version = match.group(1)
    current_documents = {
        "README.md": f"**Current native version:** {version}",
        "docs/index.md": f"**{version} native C11 implementation**",
        "docs/development/packaging.md":
            f"find_package(CentralRankingDogma {version} CONFIG REQUIRED)",
    }
    for relative, expected in current_documents.items():
        if expected not in (ROOT / relative).read_text(encoding="utf-8"):
            fail(relative, f"active native package version must be {version}")
    for relative in (
        "native/CMakeLists.txt",
        "native/include/central_ranking_dogma.h",
        "native/src/containers/hashmap.c",
        "native/src/internal",
        "native/cli/json/main.c",
        "native/cli/educational/crd_cli.c",
        "benchmarks/CMakeLists.txt",
        "tests/contracts/test_json_contract.py",
        "tests/integration/test_json_cli_native.py",
        "tests/tooling/test_commit_messages.py",
        "tests/support/calendar_test_fixture.h",
        "examples/native/ranking_batch.c",
        "research/tools/run_experiments.py",
        "docs/development/repository-layout.md",
        "docs/development/migration-3.1.4.md",
        "docs/development/local-verification.md",
    ):
        if not (ROOT / relative).exists():
            fail("repository layout", f"missing maintained path: {relative}")


def check_structure() -> None:
    for path in REF_DIR.glob("*.md"):
        text = path.read_text(encoding="utf-8")
        sections = re.findall(r"(?m)^##\s+(.+)$", text)
        if sections != REQUIRED_SECTIONS:
            fail(str(path), f"scientific sections mismatch: {sections}")
    ledger = (DOCS / "roadmap.md").read_text(encoding="utf-8")
    for i in range(1, 13):
        version = f"3.0.{i}"
        path = DOCS / "milestones" / f"{version}.md"
        if not path.exists() or f"`{version}`" not in ledger:
            fail("roadmap", f"missing {version} milestone/ledger entry")
    for version in ("3.1.1", "3.1.2", "3.1.3", "3.1.4"):
        path = DOCS / "milestones" / f"{version}.md"
        if not path.exists() or f"`{version}`" not in ledger:
            fail("roadmap", f"missing {version} planned milestone/ledger entry")
        elif version == "3.1.1":
            status = path.read_text(encoding="utf-8")
            if not any(marker in status for marker in (
                "**Status:** Planned", "**Status:** Implementation patch prepared",
                "**Status:** Implemented")):
                fail(str(path), "3.1.1 milestone status unrecognized")
        elif version == "3.1.2":
            status = path.read_text(encoding="utf-8")
            if not any(marker in status for marker in (
                "**Status:** Planned", "**Status:** Evaluation in progress",
                "**Status:** Implemented")):
                fail(str(path), "3.1.2 milestone status unrecognized")
        elif version == "3.1.3":
            status = path.read_text(encoding="utf-8")
            if not any(marker in status for marker in (
                "**Status:** Planned", "**Status:** Contract-first development",
                "**Status:** Implementation in progress", "**Status:** Implemented")):
                fail(str(path), "3.1.3 milestone status unrecognized")
        elif version == "3.1.4":
            status = path.read_text(encoding="utf-8")
            if not any(marker in status for marker in (
                "**Status:** Planned", "**Status:** Implementation in progress",
                "**Status:** Implemented")):
                fail(str(path), "3.1.4 milestone status unrecognized")
    implemented = DOCS / "milestones" / "3.1.0.md"
    if "`3.1.0`" not in ledger or "**Status:** Implemented" not in implemented.read_text(encoding="utf-8"):
        fail("roadmap", "3.1.0 must have implemented status and a ledger entry")
    if "```mermaid" not in (ROOT / "README.md").read_text(encoding="utf-8"):
        fail("README", "missing central internal C diagram")
    for required in ("system.md", "data-flow.md"):
        if "```mermaid" not in (DOCS / "architecture" / required).read_text(encoding="utf-8"):
            fail(required, "missing Mermaid architecture diagram")


def main() -> int:
    for path in docs_files():
        content = path.read_text(encoding="utf-8")
        check_unsupported_math_macros(path, content)
        check_fences(path, content)
        check_links(path, content)
        check_inline_math(path, content)
    for path in sorted((DOCS / "archive").rglob("*.md")):
        check_unsupported_math_macros(path, path.read_text(encoding="utf-8"))
    check_references()
    check_project_layout_and_version()
    check_structure()
    if ERRORS:
        print("Documentation checks failed:")
        for message in ERRORS:
            print(" - " + message)
        return 1
    print("Documentation checks passed: links, anchors, inline math, scientific sections, bibliography, milestones and diagrams.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
