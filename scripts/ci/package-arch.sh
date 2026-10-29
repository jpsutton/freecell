#!/usr/bin/env bash
# CI (build): build the Arch package from this source tree.
#   package-arch.sh <out-dir>
# Runs as root in an archlinux:base-devel container. Writes
# <out-dir>/freecell-<version>-1-x86_64.pkg.tar.zst. release.yml signs it with the
# release key; the jpsutton pacman repository (jpsutton/arch-repo) takes it from the
# release and accepts it only with that signature.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "${1:?out dir}"
out=$(realpath "$1")
pacman -Syu --noconfirm --needed base-devel cmake ninja qt6-base qt6-svg git >/dev/null
# makepkg refuses to run as root.
id builder >/dev/null 2>&1 || useradd -m builder
chown -R builder "$root"
chown builder "$out"
su builder -c "cd '$root/packaging/arch' && PKGDEST='$out' makepkg --noconfirm --cleanbuild"
ls -la "$out"
