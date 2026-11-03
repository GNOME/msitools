#!/usr/bin/env python3
# Build the msitools Windows MSI from a Meson install tree.

import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def run(cmd, **kwargs):
    print("+ " + " ".join(str(c) for c in cmd), file=sys.stderr)
    subprocess.run([str(c) for c in cmd], check=True, **kwargs)


def msi_version(version):
    """Map a project version such as 0.106 or 0.106.146-7d08-dirty to
    a valid MSI ProductVersion (major.minor.build)."""
    match = re.match(r"v?(\d+)(?:\.(\d+))?(?:\.(\d+))?", version)
    if not match:
        print(f"build-msi.py: cannot parse version {version!r}, using 0.0.0",
              file=sys.stderr)
        return "0.0.0"
    major = int(match.group(1))
    minor = int(match.group(2) or 0)
    build = int(match.group(3) or 0)
    if major > 255 or minor > 255 or build > 65535:
        print(f"build-msi.py: version {version!r} does not fit an MSI "
              "ProductVersion, clamping it", file=sys.stderr)
    return f"{min(major, 255)}.{min(minor, 255)}.{min(build, 65535)}"


def clean_include(text):
    # wixl-heat emits an XML declaration even with --include; it is not
    # valid in the middle of the main document.
    lines = [line for line in text.splitlines()
             if not line.startswith("<?xml") and not line.startswith("<?ignore")]
    return "\n".join(lines) + "\n"


def component_files(root, paths, exclude=()):
    found = []
    exclude = {Path(path) for path in exclude}
    for path in paths:
        path = Path(path)
        if not path.exists():
            continue
        if path.is_file():
            if path not in exclude and not path.is_symlink():
                found.append(str(path))
            continue
        for child in sorted(path.rglob("*")):
            if child.is_file() and not child.is_symlink() and child not in exclude:
                found.append(str(child))
    return found


def is_system_dll(name):
    name = name.lower()
    if name.startswith(("api-ms-win-", "ext-ms-win-")):
        return True
    return name in {
        "advapi32.dll", "bcrypt.dll", "comctl32.dll", "comdlg32.dll",
        "crypt32.dll", "dnsapi.dll", "gdi32.dll", "iphlpapi.dll",
        "kernel32.dll", "mpr.dll", "msvcrt.dll", "netapi32.dll",
        "ntdll.dll", "ole32.dll", "oleaut32.dll", "rpcrt4.dll",
        "secur32.dll", "setupapi.dll", "shell32.dll", "shlwapi.dll",
        "ucrtbase.dll", "user32.dll", "userenv.dll", "version.dll",
        "wldap32.dll", "winhttp.dll", "winmm.dll", "ws2_32.dll",
        "imm32.dll", "dbghelp.dll", "dwmapi.dll", "oleacc.dll",
        "shcore.dll", "uxtheme.dll", "usp10.dll", "winspool.drv",
    }


def write_empty_deps(include_dir):
    # The generated deps.wxi shadows the one in data/msi: the build passes
    # include_dir/wxi before the source data/msi directory to wixl.
    output_dir = include_dir / "wxi"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "deps.wxi").write_text(
        '<Include xmlns="http://schemas.microsoft.com/wix/2006/wi">\n'
        '  <Fragment>\n'
        '    <ComponentGroup Id="CG.Deps"></ComponentGroup>\n'
        '  </Fragment>\n'
        '</Include>\n',
        encoding="utf-8",
    )


def prepare_wxi_deps(root, include_dir, deps_wxi, wixl_dir, wixl,
                     host_system):
    """Stage the pre-generated data/wixl dependency closure."""
    include_dir.mkdir(parents=True, exist_ok=True)
    output_dir = include_dir / "wxi"
    output_dir.mkdir(parents=True, exist_ok=True)

    if host_system != "windows":
        write_empty_deps(include_dir)
        return set()

    mingw_prefix = Path(os.environ.get(
        "MINGW_PREFIX", "/usr/x86_64-w64-mingw32/sys-root/mingw"))
    if not mingw_prefix.is_dir():
        raise SystemExit(f"build-msi.py: MINGW_PREFIX not found: {mingw_prefix}")

    shutil.copy2(deps_wxi, output_dir / deps_wxi.name)

    # Let wixl apply the conditional sections in the wxi files (notably the
    # Win64 condition in gcc.wxi) and expand all <?require?> dependencies.
    wrapper = output_dir / "preprocess.wxs"
    wrapper.write_text(
        '<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">\n'
        '  <?include deps.wxi?>\n'
        '</Wix>\n',
        encoding="utf-8",
    )
    result = subprocess.run(
        [
            wixl,
            "-E",
            "-D", "Win64=yes",
            "-D", "GLIB_ARCH=win64",
            "-D", "SourceDir=@SOURCEDIR@",
            "-I", output_dir,
            "-I", wixl_dir,
            wrapper,
        ],
        text=True,
        capture_output=True,
        check=True,
    )

    source_root = ET.fromstring(result.stdout)
    staged = []
    provided = set()
    for element in source_root.iter():
        if element.tag.rsplit("}", 1)[-1] != "File":
            continue
        source = element.get("Source")
        if not source or not source.startswith("@SOURCEDIR@/"):
            raise SystemExit(f"build-msi.py: unexpected wxi source {source!r}")
        relative = source[len("@SOURCEDIR@/"):]
        source_file = mingw_prefix / relative
        if not source_file.is_file():
            raise SystemExit(
                f"build-msi.py: {relative} from data/wixl was not found in "
                f"{mingw_prefix}; update the pre-generated wxi files with "
                "data/wxi-update.py --arch x86_64 or point MINGW_PREFIX at a "
                "matching sysroot")
        target = root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_file, target)
        provided.add(target.name.lower())
        staged.append(target)

    print(f"build-msi.py: staged {len(staged)} pre-generated wxi dependencies",
          file=sys.stderr)
    return provided


def verify_dll_coverage(root, provided):
    """Use objdump to check that deps.wxi covers all non-system imports."""
    objdump = os.environ.get("MSI_OBJDUMP") or shutil.which("x86_64-w64-mingw32-objdump")
    if objdump is None:
        print("build-msi.py: no mingw objdump, skipping the dependency coverage "
              "check; set MSI_OBJDUMP to enable it", file=sys.stderr)
        return

    available = set(provided)
    available.update(p.name.lower() for p in (root / "bin").glob("*.dll"))
    missing = set()
    checked = 0

    for binary in root.rglob("*"):
        if not binary.is_file() or binary.suffix.lower() not in (".exe", ".dll"):
            continue
        checked += 1
        result = subprocess.run([objdump, "-p", str(binary)],
                                text=True, capture_output=True, check=True)
        for line in result.stdout.splitlines():
            line = line.strip()
            if not line.startswith("DLL Name:"):
                continue
            name = line.split(":", 1)[1].strip()
            if not is_system_dll(name) and name.lower() not in available:
                missing.add(name)

    if missing:
        raise SystemExit(
            "build-msi.py: dependencies missing from data/msi/deps.wxi: "
            + ", ".join(sorted(missing)))
    print(f"build-msi.py: verified dependencies for {checked} binaries",
          file=sys.stderr)


def heat_include(wixl_heat, root, include_dir, name, paths, component_group,
                 exclude=()):
    include_dir.mkdir(parents=True, exist_ok=True)
    output = include_dir / name
    files = component_files(root, paths, exclude)
    if not files:
        output.write_text(
            '<Include xmlns="http://schemas.microsoft.com/wix/2006/wi">\n'
            '  <Fragment>\n'
            f'    <ComponentGroup Id="{component_group}"></ComponentGroup>\n'
            '  </Fragment>\n'
            '</Include>\n',
            encoding="utf-8",
        )
        return

    result = subprocess.run(
        [
            str(wixl_heat),
            "-p", str(root) + "/",
            "-i",
            "--var", "var.SourceDir",
            "--directory-ref", "INSTALLDIR",
            "--component-group", component_group,
            "--win64",
        ],
        input="\n".join(files) + "\n",
        text=True,
        capture_output=True,
        check=True,
    )
    output.write_text(clean_include(result.stdout), encoding="utf-8")


def main(argv):
    if len(argv) != 10:
        print(
            "usage: build-msi.py MESON BUILD_DIR SOURCE_DIR PREFIX "
            "HOST_SYSTEM WIXL WIXL_HEAT VERSION OUTPUT",
            file=sys.stderr,
        )
        return 2

    (meson, build_dir, source_dir, prefix, host_system,
     wixl, wixl_heat, version, output) = argv[1:]
    build_dir = Path(build_dir).resolve()
    source_dir = Path(source_dir).resolve()
    output = Path(output).resolve()

    work = build_dir / "data" / "msi"
    stage = work / "root"
    include_dir = work / "include"
    for path in (stage, include_dir):
        if path.exists():
            shutil.rmtree(path)

    prefix_path = Path(prefix)
    if prefix_path == Path("/"):
        root = stage
    else:
        root = stage / prefix_path.as_posix().lstrip("/")

    try:
        run([
            meson, "install",
            "-C", build_dir,
            "--no-rebuild",
            "--destdir", stage,
            "-q",
        ])
    except subprocess.CalledProcessError:
        print("build-msi.py: meson install failed, the build tree is probably "
              f"incomplete; run `ninja -C {build_dir}` first", file=sys.stderr)
        return 1

    # These are Linux Perl frontends, not Windows programs.
    for name in ("msidiff", "msidump"):
        path = root / "bin" / name
        if path.exists():
            path.unlink()

    # The command prompt wrapper is installed by the manual component in the
    # WiX source, so keep it outside the generated component groups.
    shutil.copy2(source_dir / "data" / "msi" / "msitools.cmd", root / "msitools.cmd")

    # Generate the project component groups before dependency files are
    # staged, so CG.Core only contains files built by msitools.  wixl and
    # wixl-heat belong to WixAuthoring, not CoreTools.
    wixl_bins = [
        root / "bin" / "wixl",
        root / "bin" / "wixl.exe",
        root / "bin" / "wixl-heat",
        root / "bin" / "wixl-heat.exe",
    ]
    heat_include(wixl_heat, root, include_dir, "components-core.wxi",
                 [root / "bin"], "CG.Core", exclude=wixl_bins)
    heat_include(wixl_heat, root, include_dir, "components-wix.wxi",
                 wixl_bins + sorted((root / "share").glob("wixl-*")),
                 "CG.Wix")
    heat_include(wixl_heat, root, include_dir, "components-locale.wxi",
                 [root / "share" / "locale"], "CG.Locale")
    heat_include(wixl_heat, root, include_dir, "components-dev.wxi", [
        root / "include",
        root / "lib",
        root / "lib64",
        root / "share" / "gir-1.0",
        root / "share" / "vala",
        root / "share" / "pkgconfig",
    ], "CG.Dev")

    provided = prepare_wxi_deps(root, include_dir,
                                source_dir / "data" / "msi" / "deps.wxi",
                                source_dir / "data" / "wixl",
                                wixl,
                                host_system)
    if host_system == "windows":
        verify_dll_coverage(root, provided)

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    extra_defines = []
    for define in os.environ.get("MSI_WIXL_DEFINES", "").split():
        extra_defines += ["-D", define]

    run([
        wixl,
        "-o", output,
        "-D", "ProductVersion=" + msi_version(version),
        "-D", "SourceDir=" + str(root),
        "-D", "Win64=yes",
        "-D", "GLIB_ARCH=win64",
        *extra_defines,
        "-I", include_dir / "wxi",
        "-I", include_dir,
        "-I", source_dir / "data" / "wixl",
        "-I", source_dir / "data" / "msi",
        "--ext", "ui",
        "--extdir", source_dir / "data" / "ext",
        source_dir / "data" / "msi" / "msitools.wxs",
    ])

    if not output.exists():
        print(f"build-msi.py: {output} was not created", file=sys.stderr)
        return 1
    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
