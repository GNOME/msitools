# Build and check the msitools Windows MSI on Fedora.

default:
    @just --list

# Build msitools-v<version>.msi from a clean checkout
build-msi:
    #!/usr/bin/env bash
    set -euo pipefail

    for tool in meson ninja python3 valac bison wine \
                x86_64-w64-mingw32-gcc x86_64-w64-mingw32-pkg-config \
                x86_64-w64-mingw32-dlltool; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "error: missing $tool" >&2
            echo "install the Fedora build dependencies, e.g.:" >&2
            echo "  sudo dnf builddep msitools" >&2
            echo "  sudo dnf install mingw64-gcc mingw64-glib2-devel mingw64-libgsf-devel mingw64-libgcab1 mingw64-libxml2-devel mingw64-binutils wine" >&2
            exit 1
        fi
    done

    if [ -d build-native ]; then
        meson setup --reconfigure build-native -Dvalidate-wxi=false
    else
        meson setup build-native -Dvalidate-wxi=false
    fi
    ninja -C build-native

    wixl="$PWD/build-native/tools/wixl/wixl"
    msiinfo="$PWD/build-native/tools/msiinfo"
    msiextract="$PWD/build-native/tools/msiextract"

    if [ -d build-cross ]; then
        meson setup --reconfigure build-cross \
            --cross-file build-aux/mingw64-cross.ini \
            --prefix=/ \
            -Dmsi=true \
            -Dintrospection=true \
            -Dvalidate-wxi=false \
            -Dmsi-wixl="$wixl" \
            -Dmsi-msiinfo="$msiinfo" \
            -Dmsi-msiextract="$msiextract"
    else
        meson setup build-cross \
            --cross-file build-aux/mingw64-cross.ini \
            --prefix=/ \
            -Dmsi=true \
            -Dintrospection=true \
            -Dvalidate-wxi=false \
            -Dmsi-wixl="$wixl" \
            -Dmsi-msiinfo="$msiinfo" \
            -Dmsi-msiextract="$msiextract"
    fi

    rm -f build-cross/data/msi/msitools-v*.msi
    ninja -C build-cross
    ninja -C build-cross msitools-msi

    msi=$(find build-cross/data/msi -maxdepth 1 \
        -name 'msitools-v*.msi' -print -quit)
    if [ -z "$msi" ]; then
        echo "error: MSI was not created" >&2
        exit 1
    fi
    echo "built: $msi"

# Validate the MSI tables and install/uninstall it under Wine
check-msi: build-msi
    #!/usr/bin/env bash
    set -euo pipefail

    if ! command -v wine >/dev/null 2>&1; then
        echo "error: wine is required for check-msi" >&2
        echo "install it with: sudo dnf install wine" >&2
        exit 1
    fi

    meson test -C build-cross "msitools MSI FeatureTree" --print-errorlogs
    meson test -C build-cross "msitools MSI install smoke" --print-errorlogs
