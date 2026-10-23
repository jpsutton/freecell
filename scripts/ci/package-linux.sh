#!/usr/bin/env bash
# CI (build): turn a configured and built tree into an AppImage.
#   package-linux.sh <build-dir> <out-dir>
# Needs QT_ROOT_DIR (set by install-qt-action). Writes
# <out-dir>/FreeCell-<version>-x86_64.AppImage.
set -euo pipefail
build=$(realpath "${1:?build dir}")
out=$(realpath -m "${2:?out dir}")
: "${QT_ROOT_DIR:?QT_ROOT_DIR is not set}"
cd "$(dirname "$0")/../.."
version=$(scripts/ci/version.sh)
mkdir -p "$out"

appdir=$build/AppDir
rm -rf "$appdir"
DESTDIR=$appdir cmake --install "$build" --prefix /usr

tools=$build/linuxdeploy
mkdir -p "$tools"
base=https://github.com/linuxdeploy
curl -fsSL -o "$tools/linuxdeploy" "$base/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
curl -fsSL -o "$tools/linuxdeploy-plugin-qt" \
  "$base/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x "$tools"/*

# No FUSE on the runner: the tools run by extracting themselves.
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH=$tools:$PATH
export QMAKE=$QT_ROOT_DIR/bin/qmake
# network: the update checker's TLS plugins.
export EXTRA_QT_MODULES="svg;network"
# Native Wayland when the Qt install has it; otherwise Qt falls back to X11 (XWayland).
if [[ -f $QT_ROOT_DIR/plugins/platforms/libqwayland-generic.so ]]; then
  export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so"
fi
export LDAI_OUTPUT=$out/FreeCell-$version-x86_64.AppImage

linuxdeploy --appdir "$appdir" --plugin qt --output appimage
find "$appdir" -path '*/plugins/*' -name '*.so' | sed "s|$appdir/||" | sort
ls -la "$out"
