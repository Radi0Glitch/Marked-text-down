#!/usr/bin/env bash
# Manual release: builds, signs and publishes the GitHub release for a version tag.
#   1. checks that the tag exists and matches the version in meson.build
#   2. builds and signs the Linux archive and Windows installer (build.sh --sign --test)
#   3. creates the GitHub release with the files and their .asc signatures
#
# Usage: ./release.sh v1.2.2
# Needs: the tag created locally (git tag -a vX.Y.Z -m "..."), gh logged in, gpg key F97C07F236D69415.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

TAG="${1:-}"
if [ -z "$TAG" ]; then
    echo "usage: $0 vX.Y.Z" >&2
    exit 2
fi
VERSION="${TAG#v}"

if ! git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    echo "tag $TAG does not exist; create it first: git tag -a $TAG -m \"...\"" >&2
    exit 1
fi
if ! grep -q "version: '$VERSION'" meson.build; then
    echo "meson.build version is not $VERSION" >&2
    exit 1
fi

NOTES="$(git tag -l --format='%(contents)' "$TAG")"
if [ -z "$NOTES" ]; then
    echo "tag $TAG has no message; put the release notes in the tag annotation" >&2
    exit 1
fi

./build.sh --sign --test

FILES=(
    "dist/Marked-text-down-linux-x86_64.tar.gz"
    "dist/Marked-text-down-linux-x86_64.tar.gz.asc"
    "dist/Marked-text-down-Setup-x86_64.exe"
    "dist/Marked-text-down-Setup-x86_64.exe.asc"
)
for f in "${FILES[@]}"; do
    [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done

gh release create "$TAG" "${FILES[@]}" --title "Marked-text-down $VERSION" --notes "$NOTES"
