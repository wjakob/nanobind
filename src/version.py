#!/usr/bin/env python3

# With no command line flag, this prints the nanobind version.
# With flags -w semver, this writes the new version to where it's needed.

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

# Write the semantic version to nanobind.h, pyproject.toml, and __init__.py.
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


def main():
    root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if len(sys.argv) == 1:
        get_version(root)
    elif len(sys.argv) == 3 and sys.argv[1] == '-w':
        write_version(root, sys.argv[2])
    else:
        print("Usage: ", sys.argv[0], file=sys.stderr)
        print("   or: ", sys.argv[0], "-w X.Y.Z", file=sys.stderr)
        print("   or: ", sys.argv[0], "-w X.Y.Z-devN", file=sys.stderr)


if __name__ == '__main__':
    main()

