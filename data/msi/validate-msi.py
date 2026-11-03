#!/usr/bin/env python3
# Validate the tables and FeatureTree behaviour of a built msitools MSI.

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# Code pages Windows Installer refuses to use in the summary information: it
# keeps PID_CODEPAGE in a 16 bit field and reports error 1620 "This
# installation package could not be opened" for Unicode code pages.
UNSUPPORTED_CODEPAGES = (1200, 1201, 12000, 12001, 65000, 65001)


def resolve_tool(path):
    # shutil.which() keeps a relative path as given, and msiextract runs in a
    # temporary directory, so resolve it against the current directory
    found = shutil.which(path)
    return str(Path(found).resolve()) if found else path


def run(cmd, cwd=None):
    return subprocess.check_output([str(c) for c in cmd], text=True, cwd=cwd)


def require(text, needle, what):
    if needle not in text:
        raise SystemExit(f"missing {what}: {needle!r}")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("msiinfo", help="msiinfo used to read the tables")
    parser.add_argument("msi")
    parser.add_argument("msiextract", nargs="?", help="msiextract, if available")
    parser.add_argument("--require-wix", action="store_true",
                        help="require the WiX authoring payload in the MSI")
    parser.add_argument("--windows-payload", action="store_true",
                        help="require the mingw runtime DLLs in the MSI")
    parser.add_argument("--require-msiextract", action="store_true",
                        help="require msiextract in the payload")
    args = parser.parse_args(argv[1:])

    # the tools and the package are resolved now, because msiextract runs in
    # its own temporary directory below
    msiinfo = resolve_tool(args.msiinfo)
    msi = str(Path(args.msi).resolve())
    msiextract = None
    if args.msiextract is not None:
        msiextract = resolve_tool(args.msiextract)
    require_wix = args.require_wix

    tables = run([msiinfo, "tables", msi])
    for table in [
        "Feature", "FeatureComponents", "Dialog", "Control", "ControlEvent",
        "Environment", "Shortcut", "Upgrade", "Property", "Media", "Icon",
    ]:
        require(tables, table, "table")

    # a package without a summary information code page is still usable, but a
    # Unicode one makes msiexec refuse it outright
    suminfo = run([msiinfo, "suminfo", msi])
    match = re.search(r"^Codepage: (-?\d+)", suminfo, re.MULTILINE)
    if match is not None:
        codepage = int(match.group(1)) & 0xFFFF
        if codepage in UNSUPPORTED_CODEPAGES:
            raise SystemExit(
                f"summary information code page {codepage} is not supported by "
                "Windows Installer, use an ANSI code page such as 1252")

    feature = run([msiinfo, "export", msi, "Feature"])
    for line in [
        "Msitools\t\tmsitools\t",
        "CoreTools\tMsitools\tCommand-line tools\t",
        "WixAuthoring\tMsitools\tWiX authoring files\t",
        "Translations\tMsitools\tTranslations\t",
        "Development\tMsitools\tDevelopment files\t",
    ]:
        require(feature, line, "FeatureTree entry")

    dialog = run([msiinfo, "export", msi, "Dialog"])
    for name in [
        "CustomizeDlg", "BrowseDlg", "DiskCostDlg",
        "LicenseAgreementDlg", "VerifyReadyDlg",
    ]:
        require(dialog, name, "dialog")

    control = run([msiinfo, "export", msi, "Control"])
    require(control, "CustomizeDlg\tTree\tSelectionTree\t", "feature tree control")
    require(control, "BrowseDlg\tPathEdit\tPathEdit\t", "browse path control")

    events = run([msiinfo, "export", msi, "ControlEvent"])
    for line in [
        "WelcomeDlg\tNext\tNewDialog\tLicenseAgreementDlg\t",
        "LicenseAgreementDlg\tNext\tNewDialog\tCustomizeDlg\t",
        "CustomizeDlg\tBrowse\tSelectionBrowse\tBrowseDlg\t",
        "VerifyReadyDlg\tBack\tNewDialog\tCustomizeDlg\t",
        "MaintenanceTypeDlg\tChangeButton\tNewDialog\tCustomizeDlg\t",
    ]:
        require(events, line, "dialog transition")

    environment = run([msiinfo, "export", msi, "Environment"])
    require(environment, "MsitoolsPath\t=-*PATH\t", "PATH environment entry")
    require(environment, "[INSTALLDIR]bin", "PATH environment value")
    require(environment, "WixlDatadir\t=-*WIXL_DATADIR\t",
            "WIXL_DATADIR environment entry")
    require(environment, "[INSTALLDIR]share", "WIXL_DATADIR environment value")

    shortcut = run([msiinfo, "export", msi, "Shortcut"])
    require(shortcut, "StartMenuMsitools\tProgramMenuDir\tmsitools Command Prompt\t",
            "Start Menu shortcut")

    upgrade = run([msiinfo, "export", msi, "Upgrade"])
    require(upgrade.lower(), "7e5a9f6c-3b2d-4c8e-9f1a-6d4b2c8e0a71", "UpgradeCode")

    prop = run([msiinfo, "export", msi, "Property"])
    require(prop, "WIXUI_INSTALLDIR\tINSTALLDIR", "WIXUI_INSTALLDIR")
    require(prop, "ARPPRODUCTICON\tmsitools.ico", "ARPPRODUCTICON")

    actions = run([msiinfo, "export", msi, "CustomAction"])
    require(actions, "SetARPINSTALLLOCATION\t51\tARPINSTALLLOCATION\t[INSTALLDIR]",
            "ARPINSTALLLOCATION SetProperty action")

    files = run([msiinfo, "export", msi, "File"])
    payload = ["msitools.cmd", "msiinfo", "msibuild"]
    if args.require_msiextract:
        payload += ["msiextract"]
    for name in payload:
        require(files, name, "payload file")
    if args.windows_payload:
        for name in ["libglib-2.0-0.dll", "libgio-2.0-0.dll",
                     "libgsf-1-114.dll", "libgcab-1.0-0.dll", "zlib1.dll"]:
            require(files, name, "payload file")
        require(files, "libxml2-", "payload file")
    if require_wix:
        for name in ["wixl", "wixl-heat", "Libmsi-1.0.gir",
                     "Libmsi-1.0.typelib", "libmsi-1.0.vapi"]:
            require(files, name, "WiX payload file")

    if msiextract is not None:
        with tempfile.TemporaryDirectory() as tmp:
            # msiextract writes the Binary and Icon streams to the current
            # directory, so run it in the temporary one
            run([msiextract, "-C", tmp, msi], cwd=tmp)
            root = Path(tmp)
            if require_wix and not list(root.rglob("bin/wixl*")):
                raise SystemExit("missing extracted wixl")
            if require_wix and not list(root.rglob("share/gir-1.0/Libmsi-1.0.gir")):
                raise SystemExit("missing extracted libmsi GIR")
            if require_wix and not list(root.rglob("girepository-1.0/Libmsi-1.0.typelib")):
                raise SystemExit("missing extracted libmsi typelib")
            if require_wix and not list(root.rglob("share/vala/vapi/libmsi-1.0.vapi")):
                raise SystemExit("missing extracted libmsi VAPI")
            if not (list(root.rglob("bin/msiinfo")) +
                    list(root.rglob("bin/msiinfo.exe"))):
                raise SystemExit("missing extracted msiinfo")
            if not (list(root.rglob("bin/msiextract")) +
                    list(root.rglob("bin/msiextract.exe"))):
                raise SystemExit("missing extracted msiextract")
            if require_wix and not list(root.rglob("share/wixl-*/ext/ui/CustomizeDlg.wxs")):
                raise SystemExit("missing extracted WiX UI extension")
            if not list(root.rglob("msitools.cmd")):
                raise SystemExit("missing extracted msitools.cmd")

    print(f"{msi}: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
