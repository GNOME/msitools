#!/usr/bin/env python3
#
# Regenerate a wixl/<pkg>.wxi include file from the file list of the
# installed mingw32-<pkg> RPM, via wixl-heat.
#
# Two optional sidecar files next to wixl/<pkg>.wxi customize the
# output, and are also consumed by wxi-validate.py:
#   wixl/<pkg>.heat    extra wixl-heat arguments, e.g. "--require gettext"
#   wixl/<pkg>.ignore  extended regexes (one per line) matched against
#                       RPM-listed paths to leave out of the wxi
#
# Must be run where the mingw32-<pkg> RPM is installed and wixl-heat
# has been built (e.g. inside the project's toolbox/container).
#
# Copyright 2018-2019 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see
# <http://www.gnu.org/licenses/>.
#

import argparse
import os
import re
import shlex
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(SCRIPT_DIR)
WIXL_DIR = os.path.join(SCRIPT_DIR, "wixl")


def load_heat_args(pkg):
    heat_file = os.path.join(WIXL_DIR, f"{pkg}.heat")
    if not os.path.exists(heat_file):
        return []
    with open(heat_file) as f:
        return shlex.split(f.read())


def load_ignore_patterns(pkg):
    ignore_file = os.path.join(WIXL_DIR, f"{pkg}.ignore")
    patterns = []
    if not os.path.exists(ignore_file):
        return patterns
    with open(ignore_file) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                try:
                    patterns.append(re.compile(line))
                except re.error as e:
                    print(f"Invalid regex in {ignore_file}: {line} ({e})", file=sys.stderr)
    return patterns


def is_ignored(filepath, patterns):
    return any(pattern.search(filepath) for pattern in patterns)


def get_rpm_files(rpmname):
    result = subprocess.run(
        ["rpm", "-ql", rpmname],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.splitlines()


def find_wixl_heat():
    override = os.environ.get("WIXL_HEAT")
    if override:
        return override
    default = os.path.join(REPO_DIR, "build", "tools", "wixl", "wixl-heat")
    if os.path.isfile(default) and os.access(default, os.X_OK):
        return default
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate a wixl/<pkg>.wxi file from the installed mingw32-<pkg> RPM"
    )
    parser.add_argument("package")
    parser.add_argument(
        "--arch", default="i686", choices=["i686", "x86_64"],
        help="mingw sysroot to scan for the file list (default: i686)"
    )
    args = parser.parse_args()

    pkg = args.package
    wixl_heat = find_wixl_heat()
    if not wixl_heat:
        print(
            "wixl-heat not found (build it first, or set WIXL_HEAT)",
            file=sys.stderr,
        )
        return 1

    rpmname = f"mingw32-{pkg}"
    try:
        rpm_files = get_rpm_files(rpmname)
    except subprocess.CalledProcessError:
        print(f"{rpmname} is not installed", file=sys.stderr)
        return 1

    patterns = load_ignore_patterns(pkg)
    files = [f for f in rpm_files if not is_ignored(f, patterns)]

    out_file = os.path.join(WIXL_DIR, f"{pkg}.wxi")
    build_dir = os.path.join(REPO_DIR, "build")
    wixl_heat_argv0 = wixl_heat
    if os.path.commonpath([wixl_heat, build_dir]) == build_dir:
        wixl_heat_argv0 = os.path.relpath(wixl_heat, build_dir)
        wixl_heat = os.path.abspath(wixl_heat)
    else:
        build_dir = SCRIPT_DIR
    cmd = [
        wixl_heat_argv0, "-i", "--var", "var.SourceDir",
        "-p", f"/usr/{args.arch}-w64-mingw32/sys-root/mingw/",
        "--directory-ref", "INSTALLDIR", "--win64",
        "--component-group", f"CG.{pkg}",
    ] + load_heat_args(pkg)

    result = subprocess.run(
        cmd, input="\n".join(files), capture_output=True, text=True, cwd=build_dir
    )
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        return 1

    with open(out_file, "w") as f:
        f.write(result.stdout)

    print(f"Wrote {out_file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
