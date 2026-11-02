#!/usr/bin/env python3
# Install, run and uninstall the Windows MSI under a throw-away Wine prefix.

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(cmd, env, timeout=180):
    print("+ " + " ".join(str(c) for c in cmd), file=sys.stderr)
    return subprocess.run([str(c) for c in cmd], env=env, timeout=timeout,
                          text=True, capture_output=True, check=False)


def main(argv):
    if len(argv) != 2:
        print("usage: smoke-msi.py MSI", file=sys.stderr)
        return 2

    msi = str(Path(argv[1]).resolve())
    wine = os.environ.get("WINE", "wine")
    with tempfile.TemporaryDirectory() as tmp:
        os.chdir(tmp)
        prefix = Path(tmp) / "wine"
        env = os.environ.copy()
        env["WINEPREFIX"] = str(prefix)
        env["WINEDEBUG"] = "-all"

        result = run([wine, "msiexec", "/i", msi, "/qn",
                      "ADDLOCAL=CoreTools,WixAuthoring,Translations"], env)
        if result.returncode != 0:
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 1

        drive_c = prefix / "drive_c"
        installed = list(drive_c.rglob("msiinfo")) + list(drive_c.rglob("msiinfo.exe"))
        if not installed:
            print("smoke-msi.py: msiinfo was not installed", file=sys.stderr)
            return 1
        if not list(drive_c.rglob("msitools.cmd")):
            print("smoke-msi.py: msitools.cmd was not installed", file=sys.stderr)
            return 1
        if not list(drive_c.rglob("msitools Command Prompt.lnk")):
            print("smoke-msi.py: the Start Menu shortcut was not installed",
                  file=sys.stderr)
            return 1

        result = run([wine, installed[0], "--version"], env)
        if result.returncode != 0:
            print("smoke-msi.py: installed msiinfo failed to run", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 1
        if "msitools" not in (result.stdout + result.stderr).lower():
            print("smoke-msi.py: unexpected msiinfo --version output", file=sys.stderr)
            return 1

        for name, option in (("msiextract.exe", "--version"),
                             ("wixl.exe", "--version"),
                             ("wixl-heat.exe", "--help")):
            tool = list(drive_c.rglob(name))
            if not tool:
                print(f"smoke-msi.py: {name} was not installed", file=sys.stderr)
                return 1
            result = run([wine, tool[0], option], env)
            if result.returncode != 0:
                print(f"smoke-msi.py: installed {name} failed to run", file=sys.stderr)
                print(result.stdout, file=sys.stderr)
                print(result.stderr, file=sys.stderr)
                return 1

        result = run([wine, "msiexec", "/x", msi, "/qn"], env)
        if result.returncode != 0:
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 1

        left = list(drive_c.rglob("msiinfo")) + list(drive_c.rglob("msiinfo.exe"))
        if left:
            print("smoke-msi.py: msiinfo was not removed", file=sys.stderr)
            return 1
        if list(drive_c.rglob("msitools.cmd")):
            print("smoke-msi.py: msitools.cmd was not removed", file=sys.stderr)
            return 1
        if list(drive_c.rglob("msitools Command Prompt.lnk")):
            print("smoke-msi.py: the Start Menu shortcut was not removed",
                  file=sys.stderr)
            return 1

    print(f"{msi}: install/uninstall OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
