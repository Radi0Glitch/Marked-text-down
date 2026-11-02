#!/usr/bin/env bash
# Builds the release files locally, the same set the release workflow would build:
#   dist/Marked-text-down-linux-x86_64.tar.gz      portable Linux archive
#   dist/Marked-text-down-Setup-x86_64.exe         Windows installer (NSIS)
#
# Usage: ./build.sh [--linux] [--windows] [--sign] [--test]
#   no part flags   build both
#   --linux         only the Linux archive
#   --windows       only the Windows installer
#   --sign          sign the results with GPG key F97C07F236D69415 (asks for the passphrase)
#   --test          run --selftest on the Linux build first
#
# Needs: meson, ninja, SDL3 and SDL3_ttf (system, for Linux); for Windows also
# mingw-w64 (x86_64-w64-mingw32-g++), makensis and the SDL3 mingw packages in
# third_party/mingw (see third_party/mingw/cross.ini).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

DO_LINUX=0
DO_WINDOWS=0
DO_SIGN=0
DO_TEST=0
for arg in "$@"; do
    case "$arg" in
        --linux) DO_LINUX=1 ;;
        --windows) DO_WINDOWS=1 ;;
        --sign) DO_SIGN=1 ;;
        --test) DO_TEST=1 ;;
        -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done
if [ "$DO_LINUX" -eq 0 ] && [ "$DO_WINDOWS" -eq 0 ]; then
    DO_LINUX=1
    DO_WINDOWS=1
fi

FONTS=(
    fonts/FiraSans-*.ttf fonts/FiraMono-*.ttf
    fonts/PT_Serif-Web-*.ttf fonts/NotoSans-Regular.ttf fonts/NotoSansMath-Regular.ttf
    fonts/*OFL*.txt
)

copy_fonts() {
    mkdir -p "$1/fonts"
    cp "${FONTS[@]}" "$1/fonts/"
}

build_linux() {
    echo "== Linux build"
    meson setup build-rel --prefix=/usr --buildtype=release --reconfigure >/dev/null 2>&1 \
        || meson setup build-rel --prefix=/usr --buildtype=release
    meson compile -C build-rel

    if [ "$DO_TEST" -eq 1 ]; then
        echo "== selftest"
        (cd build-rel && SDL_VIDEODRIVER=dummy XDG_DATA_HOME="$(mktemp -d)" \
            ./marked-text-down --selftest "$(mktemp -d)/selftest.bmp" > /tmp/mtd-selftest.log 2>&1)
        echo "selftest: $(grep -ci ' ok' /tmp/mtd-selftest.log) checks passed"
    fi

    local R="dist/Marked-text-down-linux-x86_64"
    rm -rf "$R"
    mkdir -p "$R/tools/pandoc/linux-x86_64" "$R/tools/typst/linux-x86_64"
    cp build-rel/marked-text-down "$R/"
    copy_fonts "$R"
    cp README.md LICENSE.md marked-text-down.desktop "$R/"
    cp tools/pandoc/linux-x86_64/pandoc "$R/tools/pandoc/linux-x86_64/"
    cp tools/typst/linux-x86_64/typst "$R/tools/typst/linux-x86_64/"
    tar czf dist/Marked-text-down-linux-x86_64.tar.gz -C dist Marked-text-down-linux-x86_64
    rm -rf "$R"
    echo "-> dist/Marked-text-down-linux-x86_64.tar.gz"
}

build_windows() {
    echo "== Windows build"
    local CROSS="third_party/mingw/cross.ini"
    if [ ! -f "$CROSS" ]; then
        echo "missing $CROSS: SDL3 mingw packages are not set up" >&2
        exit 1
    fi
    meson setup build-win --cross-file "$CROSS" --buildtype=release --reconfigure >/dev/null 2>&1 \
        || meson setup build-win --cross-file "$CROSS" --buildtype=release
    meson compile -C build-win

    local M="third_party/mingw"
    local W="dist/Marked-text-down-windows-x86_64"
    rm -rf "$W"
    mkdir -p "$W/tools/pandoc/windows-x86_64" "$W/tools/typst/windows-x86_64"
    cp build-win/marked-text-down.exe "$W/"
    cp "$M"/SDL3-*/x86_64-w64-mingw32/bin/SDL3.dll "$M"/SDL3_ttf-*/x86_64-w64-mingw32/bin/SDL3_ttf.dll "$W/"
    copy_fonts "$W"
    cp README.md LICENSE.md marked-text-down.desktop packaging/icons/mtd-logo.ico installer.nsi "$W/"
    cp tools/pandoc/windows-x86_64/pandoc.exe "$W/tools/pandoc/windows-x86_64/"
    cp tools/typst/windows-x86_64/typst.exe "$W/tools/typst/windows-x86_64/"
    (cd "$W" && makensis -V2 installer.nsi >/tmp/mtd-nsis.log 2>&1)
    test -f dist/Marked-text-down-Setup-x86_64.exe
    rm -rf "$W"
    echo "-> dist/Marked-text-down-Setup-x86_64.exe"
}

sign_outputs() {
    echo "== signing"
    for f in dist/Marked-text-down-linux-x86_64.tar.gz dist/Marked-text-down-Setup-x86_64.exe; do
        [ -f "$f" ] || continue
        gpg --yes --armor --detach-sign -u F97C07F236D69415 "$f"
        echo "-> $f.asc"
    done
}

mkdir -p dist
[ "$DO_LINUX" -eq 1 ] && build_linux
[ "$DO_WINDOWS" -eq 1 ] && build_windows
[ "$DO_SIGN" -eq 1 ] && sign_outputs
echo "done"
