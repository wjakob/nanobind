#!/usr/bin/env python3
"""ABI boundary lint for nanobind's public headers.

The backend ('nanobind-abi') owns all persistent storage: the 'type_data' /
'func_data' structs, the 'nb_internals' state, and the accessors that reach
them. Public headers must stay on their side of the boundary -- they may build
the frozen '*_init' construction records, pass opaque 'nb_internals *' handles
through the ABI table, and call backend functions, but must never name a storage
struct, dereference 'internals', or read the type-object tail. This script fails
if such a storage/reference boundary violation appears in code (comments and
string literals are ignored).

Run from a source checkout:  python3 tests/abi_freeze_lint.py
"""

import os
import re
import sys

# (compiled regex, human-readable explanation)
FORBIDDEN = [
    (re.compile(r"\btype_data\b(?!_init)"),
     "names backend storage struct 'type_data' (use 'type_data_init')"),
    (re.compile(r"\bfunc_data\b(?!_init)"),
     "names backend storage struct 'func_data' (use 'func_data_init')"),
    (re.compile(r"\benum_type_data\b"),
     "names backend-private enum storage 'enum_type_data'"),
    (re.compile(r"\binternals\s*->"),
     "dereferences 'internals'"),
    (re.compile(r"\bsizeof\s*\(\s*nb_internals\s*\)"),
     "inspects opaque 'nb_internals' storage"),
    (re.compile(r"\bnb_type_data\s*\("),
     "calls the storage accessor 'nb_type_data()'"),
    (re.compile(r"\btype_flags_internal\b"),
     "references backend-private 'type_flags_internal'"),
]


def strip_comments_and_strings(text: str) -> str:
    """Replace comments and string/char literals with spaces (keeping newlines
    so reported line numbers stay accurate)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            while i < n and text[i] != "\n":
                i += 1
        elif two == "/*":
            i += 2
            while i < n and text[i:i + 2] != "*/":
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            i += 2
        elif c in "\"'":
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def main() -> int:
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "include", "nanobind")
    root = os.path.normpath(root)
    if not os.path.isdir(root):
        print(f"abi_freeze_lint: header directory not found: {root}")
        return 2

    violations = []
    for dirpath, _, files in os.walk(root):
        for name in files:
            if not name.endswith((".h", ".hpp")):
                continue
            path = os.path.join(dirpath, name)
            with open(path, encoding="utf-8") as f:
                code = strip_comments_and_strings(f.read())
            rel = os.path.relpath(path, os.path.join(root, "..", ".."))
            for lineno, line in enumerate(code.splitlines(), 1):
                for pattern, why in FORBIDDEN:
                    if pattern.search(line):
                        violations.append((rel, lineno, why))

    if violations:
        print("ABI boundary violations in public headers:")
        for rel, lineno, why in violations:
            print(f"  {rel}:{lineno}: {why}")
        return 1

    print("abi_freeze_lint: OK (public headers stay on the ABI boundary)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
