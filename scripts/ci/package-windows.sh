#!/usr/bin/env bash
# CI (build, Git Bash on Windows): collect FreeCell.exe, its Qt DLLs and the MSVC
# runtime into a folder and zip it.
#   package-windows.sh <build-dir> <out-dir>
# Needs QT_ROOT_DIR (install-qt-action) and VCToolsRedistDir (msvc-dev-cmd). The exe
# is unsigned here; release.yml signs it (sign-windows.sh). Writes
# <out-dir>/FreeCell-<version>-windows-x64.zip.
set -euo pipefail
build=$(cd "${1:?build dir}" && pwd)
mkdir -p "${2:?out dir}"
out=$(cd "$2" && pwd)
: "${QT_ROOT_DIR:?QT_ROOT_DIR is not set}"
: "${VCToolsRedistDir:?VCToolsRedistDir is not set; run in an MSVC developer environment}"
cd "$(dirname "$0")/../.."
version=$(scripts/ci/version.sh)

name=FreeCell-$version-windows-x64
stage=$build/$name
rm -rf "$stage"
mkdir -p "$stage"
cp "$build/FreeCell.exe" "$stage/"
"$(cygpath -u "$QT_ROOT_DIR")/bin/windeployqt.exe" --release --no-translations \
  --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw "$stage/FreeCell.exe"
# The MSVC runtime goes next to the exe, so no separate redistributable install is needed.
crt=$(find "$(cygpath -u "$VCToolsRedistDir")/x64" -maxdepth 1 -type d -name 'Microsoft.VC*.CRT' | head -n 1)
[[ -n $crt ]] || { echo "no MSVC runtime under $VCToolsRedistDir" >&2; exit 1; }
cp "$crt"/msvcp140.dll "$crt"/vcruntime140.dll "$crt"/vcruntime140_1.dll "$stage/"
cp LICENSE "$stage/LICENSE.txt"
(cd "$build" && 7z a -tzip -mx=9 "$out/$name.zip" "$name" >/dev/null)
ls -la "$out"
