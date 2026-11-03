# msitools MSI packaging

This directory builds `msitools-v<version>.msi`, a Windows x86_64 installer
for the msitools command-line tools.

The target is a standalone Windows package: the cross build stages the mingw
runtime files referenced by the pre-generated wxi closure next to the
executables (see Payload). Wine is used at build time to run the GObject
introspection dumper and at test time to install the package, but it is not a
runtime dependency.

## Justfile shortcuts

From the repository root:

```sh
just build-msi   # native wixl + cross tree + MSI
just check-msi   # build, validate tables, install/uninstall under Wine
```

## Requirements

On Fedora, the cross build needs the mingw64 toolchain and libraries:

```sh
dnf install mingw64-gcc mingw64-glib2-devel mingw64-libgsf-devel \
    mingw64-libgcab1 mingw64-libxml2-devel mingw64-binutils meson \
    ninja-build vala wine
```

A native `wixl`/`wixl-heat` from the same source tree is used to build the
MSI. The cross build generates its own Windows `wixl`/`wixl-heat` and the
libmsi GIR, typelib and VAPI, using the build-machine `g-ir-scanner` with
wine as its binary runner.

## Native packaging smoke test

The easiest way to exercise the MSI definition and `wixl-heat` integration:

```sh
meson setup build -Dmsi=true
ninja -C build
ninja -C build msitools-msi
```

The result is `build/data/msi/msitools-v<version>.msi`. This packages the
native install tree, so the payload is not a Windows binary set and the mingw
runtime checks are skipped; it is useful for checking the UI, feature tree and
generated tables.

Run the table validation:

```sh
meson test -C build "msitools MSI FeatureTree"
```

## Cross-built release MSI

```sh
meson setup build-native -Dvalidate-wxi=false
ninja -C build-native

meson setup build-cross --cross-file build-aux/mingw64-cross.ini \
    --prefix=/ \
    -Dmsi=true -Dintrospection=true -Dvalidate-wxi=false \
    -Dmsi-wixl=$PWD/build-native/tools/wixl/wixl \
    -Dmsi-msiinfo=$PWD/build-native/tools/msiinfo \
    -Dmsi-msiextract=$PWD/build-native/tools/msiextract

ninja -C build-cross
ninja -C build-cross msitools-msi
```

The `-Dmsi-*` options point at build-machine programs. The MSI target and the
validation script run on the build machine; the cross-built `wixl` and
`msiinfo` are Windows binaries and are not used for that. Without
`-Dmsi-msiinfo`, the `msitools MSI FeatureTree` test is skipped. The build
expects `wixl-heat` next to the `wixl` given by `-Dmsi-wixl`.

Build the full cross tree before building the MSI: the target stages an
already built install tree with `meson install --no-rebuild` and does not
recursively invoke `ninja`. The target depends on the built tools and always
re-runs the staging, so rebuilding the tree and then running `ninja -C
build-cross msitools-msi` packages the current binaries; only install data that
ninja does not rebuild for the target (for example translations) can lag behind.

The MSI is written to
`build-cross/data/msi/msitools-v<version>.msi`.

### Payload

The default cross build packages:

- `msiinfo.exe`, `msibuild.exe` and `msiextract.exe`
- `wixl.exe` and `wixl-heat.exe`
- `libmsi`, `libgcab` and the generated wxi dependency closure
- the bundled WiX authoring includes, UI extension and translations
- the libmsi GIR, typelib and VAPI files
- `msitools.cmd`

The wxi closure includes every file of the pre-generated dependency packages,
not just runtime DLLs: the gettext, glib and bzip2 tools, shell wrappers such
as `xml2-config`, man pages, locale catalogs, `libexec/gettext` and the libgsf
thumbnailer. They all land in `INSTALLDIR/bin` (or the matching `share`,
`libexec` subdirectory) and the `bin` directory is added to the machine-wide
`PATH`, so the installed package also exposes tools such as `gettext.exe` or
`glib-genmarshal`. This follows from the pre-generated wxi files, which cover
whole mingw packages; narrowing `data/msi/deps.wxi` is the way to trim the
payload.


### Cross introspection

The cross build uses a target `gobject-introspection-1.0.pc` shim in
`build-aux/mingw-gi` and a `pkg-config` wrapper that keeps the mingw sysroot
out of native tool paths. `libmsi/meson.build` passes
`--use-binary-wrapper=wine` to `g-ir-scanner`, sets `GI_HOST_OS=nt` so the
scanner reads mingw import libraries instead of running `ldd`, and sets
`WINEPATH` so the scanner's temporary dumper finds the target DLLs. No target
`libgirepository` is needed: the dumper is self-contained.

## Runtime DLL dependencies

`data/msi/deps.wxi` hardcodes the dependency wxi files, currently
`libgsf.wxi` and `libgcab1.wxi`. Their `<?require?>` closures in `data/wixl/`
supply the complete dependency graph: `libgsf.wxi` pulls in `bzip2`, `glib2`
and `libxml2`, `libgcab1.wxi` pulls in `glib2`, and `glib2` pulls in `libffi`,
`gettext`, `pcre2` and `zlib`.

The MSI build uses those pre-generated `data/wixl/*.wxi` files directly.
`build-msi.py` lets `wixl -E` expand the include tree with `Win64=yes` and
`GLIB_ARCH=win64`, so conditional sections such as the libgcc SEH/DW2 choice
in `gcc.wxi` are handled by wixl. The files referenced by the expanded wxi
tree are then staged from `MINGW_PREFIX`.

The checked-in wxi files track the current Fedora rawhide mingw packages. If
the local mingw sysroot has different package versions or DLL names (for
example an older `libxml2-2.dll` instead of `libxml2-16.dll`), update the wxi
files with `data/wxi-update.py --arch x86_64` or point `MINGW_PREFIX` at a
matching sysroot. The script looks for `WIXL_HEAT` and then for a built
`build-native/tools/wixl/wixl-heat` or `build/tools/wixl/wixl-heat`.

After staging, `build-msi.py` runs `x86_64-w64-mingw32-objdump -p` over every
staged `.exe` and `.dll` and fails if a non-system import is not provided by
the wxi closure or by a DLL built by msitools itself. The check is skipped
with a warning when no mingw objdump is available.

`MINGW_PREFIX` overrides the sysroot root (default
`/usr/x86_64-w64-mingw32/sys-root/mingw`) and `MSI_OBJDUMP` overrides the
objdump executable used for the coverage check.

## Installer features and UI

The MSI uses the latest wixl UI features:

- `WixUI_FeatureTree` with the feature tree, browse dialog and disk-cost
  dialog.
- Nested features `CoreTools`, `WixAuthoring`, `Translations` and
  `Development`, with the install directory configurable from the tree.
- `WixVariable` overrides for the license, banner and dialog bitmaps. All are
  declared with `Overridable="yes"`.
- `SetProperty` for `ARPINSTALLLOCATION`.
- A machine-wide `PATH` entry for the installed `bin` directory and a
  `WIXL_DATADIR` entry so `wixl` finds its authoring data.
- Start Menu shortcut to `msitools.cmd`.
- `MajorUpgrade` with same-version upgrades, a stable `UpgradeCode`, ARP icon
  and help link.

## Overriding the license and bitmaps

`wixl` defines can be passed through the `MSI_WIXL_DEFINES` environment
variable when building the target:

```sh
MSI_WIXL_DEFINES='WixUILicenseRtf=Company-License.rtf' \
    ninja -C build-cross msitools-msi
```

Because the variables are overridable, this replaces the packaged
`License.rtf`. The same works for `WixUIBannerBmp` and `WixUIDialogBmp`.

## Validation and Wine smoke test

Validate the generated tables with the native `msiinfo`/`msiextract`:

```sh
meson test -C build-cross "msitools MSI FeatureTree" --print-errorlogs
```

This runs `validate-msi.py` with the tools given through `-Dmsi-msiinfo` and
`-Dmsi-msiextract`. To validate a package without reconfiguring, run the
script directly:

```sh
python3 data/msi/validate-msi.py \
    build-native/tools/msiinfo \
    build-cross/data/msi/msitools-v*.msi \
    build-native/tools/msiextract --windows-payload
```

`--windows-payload` additionally requires the mingw runtime DLLs in the `File`
table; it is implied by the Meson test in a cross build.

On a cross build, run the install/uninstall smoke test:

```sh
meson test -C build-cross "msitools MSI install smoke"
```

The test uses a temporary `WINEPREFIX`, installs with:

```sh
wine msiexec /i msitools-v<version>.msi /qn \
    ADDLOCAL=CoreTools,WixAuthoring,Translations
```

checks that `msiinfo`, `msiextract`, `wixl`, `wixl-heat` and `msitools.cmd`
appear and that they run, then uninstalls with `wine msiexec /x ... /qn` and verifies removal.

For an interactive test, omit `/qn` and select the features in the
`WixUI_FeatureTree` dialogs.
