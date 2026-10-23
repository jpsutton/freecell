#!/usr/bin/env bash
# CI (build): bundle Qt into FreeCell.app and pack it.
#   package-macos.sh <build-dir> <out-dir>
# Needs QT_ROOT_DIR (set by install-qt-action). The bundle is only ad-hoc signed
# here; release.yml signs it with the release identity (sign-macos.sh) and makes
# the .dmg. Writes <out-dir>/FreeCell-<version>-macos-arm64.app.tar.gz.
set -euo pipefail
build=$(cd "${1:?build dir}" && pwd)
mkdir -p "${2:?out dir}"
out=$(cd "$2" && pwd)
: "${QT_ROOT_DIR:?QT_ROOT_DIR is not set}"
cd "$(dirname "$0")/../.."
version=$(scripts/ci/version.sh)

app=$build/FreeCell.app
"$QT_ROOT_DIR/bin/macdeployqt" "$app" -always-overwrite
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"
# COPYFILE_DISABLE keeps macOS's ._ metadata files out; inside a bundle they break its seal.
COPYFILE_DISABLE=1 tar -czf "$out/FreeCell-$version-macos-arm64.app.tar.gz" -C "$build" FreeCell.app
ls -la "$out"
