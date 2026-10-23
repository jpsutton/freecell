#!/usr/bin/env bash
# Installs or updates FreeCell from the latest release on macOS (Apple silicon):
#
#   curl -fsSL https://raw.githubusercontent.com/jpsutton/freecell/main/scripts/install-macos.sh | bash
#
# The app is signed with FreeCell's own certificate, not an Apple Developer ID, and
# isn't notarized, so a copy downloaded in a browser gets the quarantine attribute
# and Gatekeeper blocks it. curl doesn't set that attribute, so the app this script
# installs opens without a prompt.
#
# The download is checked against the release's SHA256SUMS, and the app's signature
# against FreeCell's certificate (packaging/freecell-codesign.crt). Installs to
# /Applications, or ~/Applications when /Applications isn't writable. Set
# FREECELL_INSTALL_DIR to choose another folder.
set -euo pipefail

base=https://github.com/jpsutton/freecell/releases/latest/download
# SHA-256 of packaging/freecell-codesign.crt (DER).
cert_sha256=fa9462ba7ef9eed66ad592c51329f3300ef140cfb56ecd5c92c01c0990998c47

die() { echo "install-macos: $*" >&2; exit 1; }

[[ $(uname -s) == Darwin ]] || die "this script is for macOS"
[[ $(uname -m) == arm64 ]] || die "FreeCell ships for Apple silicon (arm64) only"

if [[ -n ${FREECELL_INSTALL_DIR:-} ]]; then
  dest=$FREECELL_INSTALL_DIR
elif [[ -w /Applications ]]; then
  dest=/Applications
else
  dest=$HOME/Applications
fi
mkdir -p "$dest"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

curl -fsSL -o "$work/SHA256SUMS" "$base/SHA256SUMS"
file=$(grep -oE 'FreeCell-[0-9][^ ]*-macos-arm64\.app\.tar\.gz$' "$work/SHA256SUMS" | head -n 1) \
  || die "no macOS build in the latest release"

echo "Downloading $file"
curl -fL --progress-bar -o "$work/$file" "$base/$file"
(cd "$work" && grep " $file\$" SHA256SUMS | shasum -a 256 -c -s) || die "checksum mismatch for $file"

tar -xzf "$work/$file" -C "$work"
app=$work/FreeCell.app
[[ -d $app ]] || die "FreeCell.app not found in $file"
codesign --verify --deep --strict "$app" 2>/dev/null || die "FreeCell.app's signature is broken"
(cd "$work" && codesign -d --extract-certificates=cert. "$app" 2>/dev/null) || die "FreeCell.app isn't signed"
[[ $(shasum -a 256 "$work/cert.0" | cut -d' ' -f1) == "$cert_sha256" ]] \
  || die "FreeCell.app isn't signed with FreeCell's certificate"

if pgrep -xq FreeCell; then
  echo "FreeCell is running. Quit and reopen it to use the new version."
fi
rm -rf "$dest/FreeCell.app"
ditto "$app" "$dest/FreeCell.app"

echo "Installed $dest/FreeCell.app"
