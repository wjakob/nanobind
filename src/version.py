#!/usr/bin/env python3

# With no command line flag, this prints the nanobind and nanobind-backend
# versions. With flags -w semver, this writes the new nanobind version to where
# it's needed. With flags -b semver, it does the same for the nanobind-backend
# version.

import os
import re
import sys

# Parse the header file <nanobind/nanobind.h> and print the version.
def get_version(root):
    major = ''
    minor = ''
    patch = ''
    dev = ''
    with open(os.path.join(root, "include/nanobind/nanobind.h"), 'r') as f:
        for line in f:
            if m := re.match(r'#define NB_VERSION_(.+)', line):
                if m_major := re.match(r'MAJOR\s+([0-9]+)', m.group(1)):
                    major = m_major.group(1)
                if m_minor := re.match(r'MINOR\s+([0-9]+)', m.group(1)):
                    minor = m_minor.group(1)
                if m_patch := re.match(r'PATCH\s+([0-9]+)', m.group(1)):
                    patch = m_patch.group(1)
                if m_dev := re.match(r'DEV\s+([0-9]+)', m.group(1)):
                    dev = m_dev.group(1)

    version_core = '.'.join([major, minor, patch])
    if int(dev) > 0:
        print(version_core, '-dev', dev, sep='')
    else:
        print(version_core)

# Parse nanobind-backend/pyproject.toml and print the backend version
def get_backend_version(root):
    with open(os.path.join(root, "nanobind-backend/pyproject.toml"), 'r') as f:
        m = re.search(r'^version\s*=\s*"([^"]+)"', f.read(), re.MULTILINE)
    print('nanobind-backend', m.group(1))

# Write the semantic version to nanobind.h, pyproject.toml, __init__.py,
# and docs/bazel.rst.
# The semver string must be either 'X.Y.Z' or 'X.Y.Z-devN', where X, Y, Z are
# non-negative integers and N is a positive integer.
def write_version(root, semver):
    major = 0
    minor = 0
    patch = 0
    dev = 0
    try:
        beginning, middle, end = semver.split('.', maxsplit=2)
        major = int(beginning)
        minor = int(middle)
        if m := re.match(r'([0-9]+)-dev([1-9][0-9]*)', end):
            patch = int(m.group(1))
            dev = int(m.group(2))
        else:
            patch = int(end)
    except:
        print("Invalid version: '", semver, "'", sep='', file=sys.stderr)
        print("Valid examples: '1.2.3' or '1.2.3-dev4'", file=sys.stderr)
        return

    # Write to nanobind.h
    with open(os.path.join(root, "include/nanobind/nanobind.h"), "r+") as f:
        contents = f.read()
        contents = re.sub(r'#define NB_VERSION_MAJOR\s+[0-9]+',
                          r'#define NB_VERSION_MAJOR ' + str(major),
                          contents, count=1)
        contents = re.sub(r'#define NB_VERSION_MINOR\s+[0-9]+',
                          r'#define NB_VERSION_MINOR ' + str(minor),
                          contents, count=1)
        contents = re.sub(r'#define NB_VERSION_PATCH\s+[0-9]+',
                          r'#define NB_VERSION_PATCH ' + str(patch),
                          contents, count=1)
        contents = re.sub(r'#define NB_VERSION_DEV\s+[0-9]+',
                          r'#define NB_VERSION_DEV   ' + str(dev),
                          contents, count=1)
        f.seek(0)
        f.truncate()
        f.write(contents)

    # Write to pyproject.toml
    with open(os.path.join(root, "pyproject.toml"), "r+") as f:
        contents = f.read()
        contents = re.sub(r'version\s+=\s+"[^"]+"',
                          r'version = "' + semver + '"',
                          contents, count=1)
        f.seek(0)
        f.truncate()
        f.write(contents)

    # Write to __init__.py
    with open(os.path.join(root, "src/__init__.py"), "r+") as f:
        contents = f.read()
        contents = re.sub(r'__version__\s+=\s+"[^"]+"',
                          r'__version__ = "' + semver + '"',
                          contents, count=1)
        f.seek(0)
        f.truncate()
        f.write(contents)

    # write to docs/bazel.rst, but only if `semver` is not a dev release.
    # This is because documentation is scoped only to the latest stable release.
    if "dev" not in semver:
        with open(os.path.join(root, "docs/bazel.rst"), "r+") as f:
            contents = f.read()
            contents = re.sub(
                r"nanobind\s+v\d+(\.\d+)+",
                r"nanobind v" + semver,
                contents,
                count=1,
            )
            contents = re.sub(
                r'"nanobind_bazel", version = "\d+(\.\d+)+"',
                r'"nanobind_bazel", version = "' + semver + '"',
                contents,
                count=1,
            )
            f.seek(0)
            f.truncate()
            f.write(contents)


# Write the nanobind-backend version to nb_backend.h (backend ABI major/minor
# and revision), nanobind-backend/pyproject.toml, and the documented
# ``nanobind-backend>=MAJOR.MINOR`` requirement. The version string must be
# either 'A.B.R' or 'A.B.R.devN'.
def write_backend_version(root, version):
    m = re.fullmatch(r'([0-9]+)\.([0-9]+)\.([0-9]+)(\.dev[1-9][0-9]*)?',
                     version)
    if not m:
        print("Invalid backend version: '", version, "'", sep='',
              file=sys.stderr)
        print("Valid examples: '1.0.0' or '1.0.0.dev1'", file=sys.stderr)
        return
    major, minor, revision = m.group(1), m.group(2), m.group(3)

    # Write to nb_backend.h
    with open(os.path.join(root, "include/nanobind/nb_backend.h"), "r+") as f:
        contents = f.read()
        for name, value in (('ABI_MAJOR', major), ('ABI_MINOR', minor),
                            ('REVISION', revision)):
            contents = re.sub(r'#define NB_BACKEND_%s\s+[0-9]+' % name,
                              r'#define NB_BACKEND_%s %s' % (name, value),
                              contents, count=1)
        f.seek(0)
        f.truncate()
        f.write(contents)

    # Write to nanobind-backend/pyproject.toml
    with open(os.path.join(root, "nanobind-backend/pyproject.toml"), "r+") as f:
        contents = f.read()
        contents = re.sub(r'^version\s*=\s*"[^"]+"',
                          r'version = "' + version + '"',
                          contents, count=1, flags=re.MULTILINE)
        f.seek(0)
        f.truncate()
        f.write(contents)

    # Write the documented requirement, which names the ABI version only
    for fname in ("docs/split_mode.rst", "docs/packaging.rst"):
        with open(os.path.join(root, fname), "r+") as f:
            contents = f.read()
            contents = re.sub(r'nanobind-backend>=[0-9]+\.[0-9]+',
                              r'nanobind-backend>=' + major + '.' + minor,
                              contents)
            f.seek(0)
            f.truncate()
            f.write(contents)


def main():
    root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if len(sys.argv) == 1:
        get_version(root)
        get_backend_version(root)
    elif len(sys.argv) == 3 and sys.argv[1] == '-w':
        write_version(root, sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == '-b':
        write_backend_version(root, sys.argv[2])
    else:
        print("Usage: ", sys.argv[0], file=sys.stderr)
        print("   or: ", sys.argv[0], "-w X.Y.Z", file=sys.stderr)
        print("   or: ", sys.argv[0], "-w X.Y.Z-devN", file=sys.stderr)
        print("   or: ", sys.argv[0], "-b A.B.R", file=sys.stderr)
        print("   or: ", sys.argv[0], "-b A.B.R.devN", file=sys.stderr)


if __name__ == '__main__':
    main()
