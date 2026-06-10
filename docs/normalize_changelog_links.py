#!/usr/bin/env python3
"""Normalize commit links in docs/changelog.rst.

Rewrites RST commit references to a canonical form::

    `abc123 <https://github.com/wjakob/nanobind/commit/<40-char SHA>>`__

- The link text is a 6-character short hash.
- The URL always contains the full 40-character SHA.
- Line-wrapped links (hash on one line, ``<URL>`` on the next) are joined.

Each commit is verified by running ``git rev-parse`` against the local
repository. Unresolved commits are reported as broken. Backtick hashes
that appear without a URL are also flagged so the user can add links
manually.

Paths are resolved relative to the repository root (the parent of this
script's directory), so the script can be invoked from anywhere::

    python3 docs/normalize_changelog_links.py
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = REPO_ROOT / "docs/changelog.rst"

# RST link: `TEXT <URL>`__. TEXT may contain whitespace / newlines (line wrap).
LINK_RE = re.compile(r"`([^`<]+?)\s*<([^>]+)>`__", re.DOTALL)

# Only `.../commit/<hash>` URLs are normalized. Pull-request and tag URLs are
# left alone.
COMMIT_URL_RE = re.compile(
    r"^https://github\.com/wjakob/nanobind/commit/([0-9a-f]+)/?$"
)

# Bare backtick hashes like `fdc7cae7` without a following URL. We only flag
# ones introduced by the word "commit"/"commits" to avoid false positives on
# arbitrary code snippets.
BARE_HASH_RE = re.compile(
    r"commits?\s+`([0-9a-f]{6,40})`(?!\s*<)", re.IGNORECASE
)

HEX_RE = re.compile(r"^[0-9a-f]+$")


def git_resolve(rev: str) -> str | None:
    """Return the full 40-char SHA for ``rev``, or None."""
    try:
        result = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "--verify",
             f"{rev}^{{commit}}"],
            check=True, capture_output=True, text=True,
        )
    except subprocess.CalledProcessError:
        return None
    sha = result.stdout.strip()
    return sha if len(sha) == 40 else None


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def main() -> int:
    if not CHANGELOG.is_file():
        print(f"error: {CHANGELOG} not found", file=sys.stderr)
        return 2

    text = CHANGELOG.read_text()

    broken: list[tuple[int, str, str]] = []  # (line, original, reason)

    def replace(m: re.Match) -> str:
        raw_text = m.group(1)
        url = m.group(2).strip()
        link_text = re.sub(r"\s+", "", raw_text)
        line = line_of(text, m.start())

        cm = COMMIT_URL_RE.match(url)
        if not cm:
            return m.group(0)  # not a commit URL — leave unchanged

        url_hash = cm.group(1)
        candidates: list[str] = []
        if HEX_RE.match(link_text) and len(link_text) >= 4:
            candidates.append(link_text)
        if url_hash not in candidates:
            candidates.append(url_hash)

        full_sha = None
        for cand in candidates:
            full_sha = git_resolve(cand)
            if full_sha:
                break

        if full_sha is None:
            broken.append((line, m.group(0),
                           f"no commit matching text={link_text!r} "
                           f"or url={url_hash!r}"))
            return m.group(0)

        short = full_sha[:6]
        new_url = f"https://github.com/wjakob/nanobind/commit/{full_sha}"
        return f"`{short} <{new_url}>`__"

    new_text = LINK_RE.sub(replace, text)

    # Collect bare `<hash>` refs that have no URL.
    bare: list[tuple[int, str, str, str | None]] = []
    for m in BARE_HASH_RE.finditer(new_text):
        hash_ = m.group(1)
        line = line_of(new_text, m.start())
        bare.append((line, m.group(0), hash_, git_resolve(hash_)))

    changed = new_text != text
    if changed:
        CHANGELOG.write_text(new_text)

    print(f"{CHANGELOG}: {'updated' if changed else 'no changes'}")
    print()

    if broken:
        print(f"BROKEN OR UNRESOLVED COMMIT LINKS ({len(broken)}):")
        for line, orig, reason in broken:
            snippet = " ".join(orig.split())
            print(f"  line {line}: {snippet}")
            print(f"    -> {reason}")
        print()
    else:
        print("No broken commit links.\n")

    if bare:
        print(f"BARE COMMIT HASHES WITHOUT URL ({len(bare)}):")
        for line, snippet, hash_, sha in bare:
            print(f"  line {line}: {snippet!r}")
            if sha:
                print(f"    resolves to {sha}")
            else:
                print("    not found in this repository")
        print()
    else:
        print("No bare commit hashes found.\n")

    return 0 if not broken else 1


if __name__ == "__main__":
    sys.exit(main())
