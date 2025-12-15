#!/usr/bin/env python3
#
# Validate a set of wxi files to determine if they
# list all the DLL dependencies required by any EXE
# or DLL files that contain and that all listed files
# do exist on disk
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

import os
import re
import subprocess
import sys
from xml.etree import ElementTree as ET

# WXI Filename -> group name
filegroups = {}

# Group name -> dict (file, array of component groups, array of component hashes)
groups = {}

# Component hash -> file paths
components = {}

# DLL file name -> component hash
dllcomponents = {}
# DLL file name -> group name
dllgroups = {}

# Cache for RPM install checks
rpmcache = {}

# Cache for RPM file lists
rpmfiles_cache = {}

# Files listed in each WXI (wxi file -> set of file paths)
wxi_files = {}

# RPM info per WXI file (wxi file -> dict with rpm names and availability)
wxi_rpm_info = {}

# Ignore patterns per WXI file (wxi file -> list of compiled regexps)
wxi_ignore_patterns = {}

# DLL files provided by Windows runtime which
# thus don't need to be listed in wxi files
# as dependencies
dllbuiltin = re.compile(
    r"^("
    r"advapi32|"
    r"api-ms-.*|"
    r"avrt|"
    r"bcrypt|"
    r"bcryptprimitives|"
    r"comctl32|"
    r"comdlg32|"
    r"crypt32|"
    r"dbghelp|"
    r"dwrite|"
    r"d2d1|"
    r"d3d10|"
    r"d3d11|"
    r"d3d12|"
    r"d3d9|"
    r"dnsapi|"
    r"dsound|"
    r"dwmapi|"
    r"dxgi|"
    r"gdi32|"
    r"gdiplus|"
    r"hid|"
    r"imm32|"
    r"iphlpapi|"
    r"kernel32|"
    r"ksuser|"
    r"mf|"
    r"mfplat|"
    r"mfreadwrite|"
    r"msimg32|"
    r"msvcrt|"
    r"mswsock|"
    r"ncrypt|"
    r"ntdll|"
    r"ole32|"
    r"oleaut32|"
    r"opengl32|"
    r"psapi|"
    r"secur32|"
    r"setupapi|"
    r"shell32|"
    r"shlwapi|"
    r"user32|"
    r"userenv|"
    r"usp10|"
    r"version|"
    r"winmm|"
    r"wldap32|"
    r"ws2_32|"
    r").dll$"
)

drvbuiltin = re.compile(
    r"^("
    r"winspool|"
    r").drv$"
)

errors = 0


def load_ignore_patterns(wxi_file):
    """Load ignore patterns from a corresponding .ignore file."""
    ignore_file = wxi_file.replace(".wxi", ".ignore")
    patterns = []

    if os.path.exists(ignore_file):
        with open(ignore_file, "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    try:
                        patterns.append(re.compile(line))
                    except re.error as e:
                        print(f"Invalid regex in {ignore_file}: {line} ({e})", file=sys.stderr)

    wxi_ignore_patterns[wxi_file] = patterns


def check_rpm(rpmname):
    """Check if an RPM is installed, with caching."""
    if rpmname in rpmcache:
        return rpmcache[rpmname]
    result = subprocess.run(
        ["rpm", "-q", rpmname],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    ).returncode == 0
    rpmcache[rpmname] = result
    return result


def get_rpm_files(rpmname):
    """Get list of files from an RPM, with caching."""
    if rpmname in rpmfiles_cache:
        return rpmfiles_cache[rpmname]

    result = subprocess.run(
        ["rpm", "-ql", rpmname],
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        rpmfiles_cache[rpmname] = set()
        return set()

    # Filter for relevant files (DLLs, EXEs) in the mingw directories
    files = set()
    for line in result.stdout.splitlines():
        # Only include actual files (DLLs, EXEs, and other distributable files)
        if re.search(r"\.(dll|exe)$", line, re.IGNORECASE):
            files.add(line)
    rpmfiles_cache[rpmname] = files
    return files


def load(file):
    """Load a wxi file and extract component groups and files."""
    global errors

    # Since we're recursively expanding <require> imports
    # we might be called repeatedly for the same file, so
    # short circuit if that happens
    if file in filegroups:
        return filegroups[file]

    # By convention the .wxi files must be named to match
    # the mingw RPM name
    rpmname = os.path.basename(file)
    rpmname = rpmname.replace(".wxi", "")

    rpmname32 = f"mingw32-{rpmname}"
    rpmname64 = f"mingw64-{rpmname}"

    # This testing is only going to work if the RPMs are
    # actually installed
    hasrpm32 = check_rpm(rpmname32)
    hasrpm64 = check_rpm(rpmname64)

    if not hasrpm32:
        errors = 1
        print(f"Cannot check {file}, {rpmname32} not installed")

    if not hasrpm64:
        errors = 1
        print(f"Cannot check {file}, {rpmname64} not installed")

    # Load ignore patterns for this wxi file
    load_ignore_patterns(file)

    # Initialize file tracking
    wxi_files[file] = set()
    wxi_rpm_info[file] = {
        "rpmname32": rpmname32,
        "rpmname64": rpmname64,
        "hasrpm32": hasrpm32,
        "hasrpm64": hasrpm64,
    }

    # Pre-emptively load any includes so we can report errors
    # directly, and setup the right file groups
    with open(file, "r") as f:
        for line in f:
            match = re.search(r"<\?require (\S+)\?>", line)
            if match:
                incname = match.group(1)
                path = os.path.dirname(file)
                if path:
                    incfile = os.path.join(path, incname)
                else:
                    incfile = incname
                load(incfile)

    # Parse the XML file
    try:
        tree = ET.parse(file)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"Parsing failed on {file}: {e}", file=sys.stderr)
        errors = 1
        return None

    # Find namespace prefix if any (e.g., "{http://schemas.microsoft.com/wix/2006/wi}")
    ns = ""
    if root.tag.startswith("{"):
        ns = root.tag.split("}")[0] + "}"

    # Extract all the listed dependencies (ComponentGroup)
    cg = root.find(f".//{ns}ComponentGroup")
    if cg is None:
        print(f"No ComponentGroup found in {file}", file=sys.stderr)
        errors = 1
        return None

    cgid = cg.get("Id")
    groups[cgid] = {
        "file": file,
        "groups": [],
        "components": [],
    }

    for child in cg:
        tag = child.tag.replace(ns, "") if ns else child.tag
        if tag == "ComponentRef":
            # These are file local dependencies
            groups[cgid]["components"].append(child.get("Id"))
        elif tag == "ComponentGroupRef":
            # These are external file dependencies
            groups[cgid]["groups"].append(child.get("Id"))

    # Extract all files listed in this wxi manifest (Component elements)
    for node in root.iter(f"{ns}Component"):
        comp_id = node.get("Id")

        for child in node:
            tag = child.tag.replace(ns, "") if ns else child.tag
            if tag != "File":
                continue

            fname = child.get("Source")
            if fname is None:
                continue

            msifile32 = fname
            msifile64 = fname

            msifile32 = msifile32.replace("$(var.GLIB_ARCH)", "win32")
            msifile64 = msifile64.replace("$(var.GLIB_ARCH)", "win64")

            msifile32 = msifile32.replace(
                "$(var.SourceDir)", "/usr/i686-w64-mingw32/sys-root/mingw"
            )
            msifile64 = msifile64.replace(
                "$(var.SourceDir)", "/usr/x86_64-w64-mingw32/sys-root/mingw"
            )

            # Hacks because we don't parse conditionals

            # gcc
            if re.search(r"libgcc_s_seh-1\.dll", msifile32):
                msifile32 = None
            if re.search(r"libgcc_s_dw2-1\.dll", msifile64):
                msifile64 = None

            # openssl
            if msifile32 and re.search(r"lib(ssl|crypto)-[^-]+-x64\.dll", msifile32):
                msifile32 = None
            if msifile64 and re.search(r"lib(ssl|crypto)-[^-]+\.dll", msifile64):
                msifile64 = None

            # gstreamer1-plugins-base
            if msifile32 and "x86_64" in msifile32:
                msifile32 = None
            if msifile64 and "i686" in msifile64:
                msifile64 = None

            # Check that the listed files really do exist
            if hasrpm32 and msifile32 and not os.path.exists(msifile32):
                if not errors:
                    print(f"\n\nErrors in {file}\n")
                errors = 1
                print(f"  - Missing {msifile32}")

            if hasrpm64 and msifile64 and not os.path.exists(msifile64):
                if not errors:
                    print(f"\n\nErrors in {file}\n")
                errors = 1
                print(f"  - Missing {msifile64}")

            # Setup mappings for the component and file
            if comp_id not in components:
                components[comp_id] = []
            if msifile32:
                components[comp_id].append(msifile32)
                wxi_files[file].add(msifile32)
            if msifile64:
                components[comp_id].append(msifile64)
                wxi_files[file].add(msifile64)

            # Setup mappings for the DLLs
            dll_match = re.search(r".*/([^/]+\.dll)$", fname)
            if dll_match:
                dllname = dll_match.group(1).lower()
                dllcomponents[dllname] = comp_id
                dllgroups[dllname] = cgid

    filegroups[file] = cgid
    return cgid


def expand(name, allowed):
    """Recursively expand a file to find the full transitive set of dependencies."""
    if name not in groups:
        return

    for comp in groups[name]["components"]:
        allowed[comp] = True

    for ref in groups[name]["groups"]:
        expand(ref, allowed)


def check(name, allowed):
    """Check that all DLL/EXE files have their dependencies satisfied."""
    global errors

    if name not in groups:
        return

    for comp in groups[name]["components"]:
        if comp not in components:
            errors = 1
            print(f"Referenced component {comp} does not exist in {name}")
            return

        files = components[comp]

        for file in files:
            if not re.search(r"\.(exe|dll)$", file):
                continue

            problems = 0

            if not os.path.isfile(file):
                problems += 1
                if problems == 1:
                    print(f" > {file}")
                print(f"     - Cannot analyse missing file {file}")
                continue

            # Get the list of dependencies from objdump
            try:
                result = subprocess.run(
                    ["x86_64-w64-mingw32-objdump", "-p", file],
                    capture_output=True,
                    text=True
                )
                output = result.stdout
            except FileNotFoundError:
                print(f"     - objdump not found, cannot analyze {file}")
                continue

            for line in output.splitlines():
                match = re.search(r"DLL Name: (\S+)", line)
                if not match:
                    continue

                dllname = match.group(1).lower()

                if dllbuiltin.search(dllname):
                    continue
                if drvbuiltin.search(dllname):
                    continue

                if dllname not in dllcomponents:
                    if problems == 0:
                        print(f" > {file}")
                    problems += 1
                    print(f"     - Unknown component for {dllname}")
                    continue

                dllcomp = dllcomponents[dllname]

                if dllcomp in allowed:
                    continue

                dllgroup = dllgroups[dllname]

                if problems == 0:
                    print(f" > {file}")
                problems += 1
                print(f"     - Missing group ref {dllgroup} for {dllname}")

            if problems:
                errors = 1


def is_ignored(filepath, patterns):
    """Check if a file path matches any of the ignore patterns."""
    for pattern in patterns:
        if pattern.search(filepath):
            return True
    return False


def check_unlisted_files(wxi_file):
    """Check for files in the RPM that are not listed in the WXI."""
    global errors

    if wxi_file not in wxi_rpm_info:
        return

    info = wxi_rpm_info[wxi_file]
    listed_files = wxi_files.get(wxi_file, set())
    ignore_patterns = wxi_ignore_patterns.get(wxi_file, [])

    problems = []

    # Check 32-bit RPM
    if info["hasrpm32"]:
        rpm_files = get_rpm_files(info["rpmname32"])
        for rpm_file in sorted(rpm_files):
            if rpm_file not in listed_files and not is_ignored(rpm_file, ignore_patterns):
                problems.append(rpm_file)

    # Check 64-bit RPM
    if info["hasrpm64"]:
        rpm_files = get_rpm_files(info["rpmname64"])
        for rpm_file in sorted(rpm_files):
            if rpm_file not in listed_files and not is_ignored(rpm_file, ignore_patterns):
                problems.append(rpm_file)

    if problems:
        errors = 1
        print(f"\nFiles in RPM but not listed in {wxi_file}:")
        for f in problems:
            print(f"  - {f}")


def main():
    global errors

    if len(sys.argv) < 2:
        print("Usage: wxi-validate.py <file.wxi> [file2.wxi ...]", file=sys.stderr)
        sys.exit(1)

    checkgroups = []

    # Load all the requested wxi files, doing
    # a basic sanity check that all files in
    # the manifest exist
    for file in sys.argv[1:]:
        group = load(file)
        if group:
            checkgroups.append(group)

    if errors:
        sys.exit(1)

    # Check all DLL/EXE files to see if they
    # reference DLLs which are not mentioned as
    # dependencies
    for group in checkgroups:
        allowedcomps = {}
        expand(group, allowedcomps)
        check(group, allowedcomps)

    # Check for files in RPM but not listed in WXI
    for file in sys.argv[1:]:
        check_unlisted_files(file)

    sys.exit(errors)


if __name__ == "__main__":
    main()
