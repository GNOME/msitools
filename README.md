# msitools

## Introduction

*msitools* is a set of programs to inspect and build Windows Installer
(.MSI) files.  It is based on `libmsi`, a portable library to read and
write .MSI files.  libmsi in turn is a port of (and a subset of)
[Wine](https://www.winehq.org/)'s implementation of the Windows
Installer.

*msitools* plans to be a solution for packaging and deployment of
cross-compiled Windows applications.

## Tools

Provided tools include:

- `msiinfo`, to inspect MSI files

- `msibuild`, a low-level tool to create MSI files

- `msidiff`, compares contents of two MSI files with diff

- `msidump`, dumps raw MSI tables and stream content

- `msiextract`, to inspect and extract the files of an MSI file

- `wixl`, a WiX-like tool, that builds Windows Installer (MSI)
  packages from an XML document, and tries to share the same syntax as
  the [WiX toolset](http://wixtoolset.org/)

- `wixl-heat`, a tool that builds XML fragments from a list of files
  and directories.


## Building

*msitools* is built with [Meson](https://mesonbuild.com/). You will need:

- a C compiler, `pkg-config`, Meson >= 1.4 and Ninja

- a Vala compiler (`valac`)

- `bison` and `perl`

- GLib, GObject and GIO >= 2.68

- libgsf, libgcab >= 0.1.10 and libxml2

- gobject-introspection and the GLib GIR files (on Debian/Ubuntu:
  `libgirepository1.0-dev`), unless introspection is disabled

- the [bats-core](https://github.com/bats-core/bats-core) submodule, to
  run the tests

The test suite lives in a git submodule, so clone recursively:

    git clone --recursive https://gitlab.gnome.org/GNOME/msitools.git
    cd msitools

In an existing checkout, fetch it with:

    git submodule update --init --recursive

Configure and build in a separate build directory:

    meson setup _build
    meson compile -C _build

Run the tests and install with:

    meson test -C _build
    meson install -C _build

The default build uses GObject introspection and also builds the
Vala-based tools (`wixl`, `wixl-heat`, `msiextract`). Pass
`-Dintrospection=false` to `meson setup` to build only the C tools
(`msiinfo`, `msibuild`, `msidump`, `msidiff`) without the
introspection dependencies:

    meson setup _build -Dintrospection=false

Without the GLib GIR file, the default build fails while generating
`Libmsi-1.0.gir` with:

    Couldn't find include 'GLib-2.0.gir'

Distributions provide the dependencies, for example:

    sudo dnf builddep msitools
    sudo apt build-dep msitools


## Notes

Right now, *msitools* does not work under Windows.  It is planned that
it will self-host.

While in a very early stage, it is already usable.

*msitools* uses [libgsf](https://gitlab.gnome.org/GNOME/libgsf) in
order to read OLE Structured Storage files (which are the underlying
format of .MSI files).

Wixl lacks many features compared to WiX. As always, contributions
are welcome!

Reporting issues and sending pull requests is welcome!
