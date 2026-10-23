#!/usr/bin/env bash
# CI (release, macOS): sign FreeCell.app with the release identity and make the .dmg.
#   sign-macos.sh <dir>
# <dir> holds FreeCell-<version>-macos-arm64.app.tar.gz from the build. Needs
# FREECELL_CODESIGN_P12 (base64 PKCS#12 with the code-signing certificate and key)
# and FREECELL_CODESIGN_PASSWORD. The certificate is self-signed, not an Apple
# Developer ID, so Gatekeeper still calls the app unverified; what the signature
# gives is a stable identity that the in-app updater and install-macos.sh check.
# Replaces the tarball with the signed one and adds FreeCell-<version>-macos-arm64.dmg.
set -euo pipefail
dir=$(cd "${1:?asset dir}" && pwd)
: "${FREECELL_CODESIGN_P12:?FREECELL_CODESIGN_P12 is not set}"
: "${FREECELL_CODESIGN_PASSWORD:?FREECELL_CODESIGN_PASSWORD is not set}"

tarball=$(find "$dir" -maxdepth 1 -name 'FreeCell-*-macos-arm64.app.tar.gz' | head -n 1)
[[ -n $tarball ]] || { echo "no macOS tarball in $dir" >&2; exit 1; }
base=${tarball%.app.tar.gz}

work=$(mktemp -d)
kc=$work/signing.keychain-db
kcpass=$(uuidgen)
cleanup() {
  security delete-keychain "$kc" 2>/dev/null || true
  rm -rf "$work"
}
trap cleanup EXIT

printf '%s' "$FREECELL_CODESIGN_P12" | base64 -D > "$work/identity.p12"
security create-keychain -p "$kcpass" "$kc"
security set-keychain-settings "$kc"
security unlock-keychain -p "$kcpass" "$kc"
security import "$work/identity.p12" -k "$kc" -P "$FREECELL_CODESIGN_PASSWORD" -T /usr/bin/codesign >/dev/null
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$kcpass" "$kc" >/dev/null
# codesign looks identities up in the search list.
existing=$(security list-keychains -d user | tr -d '"')
# shellcheck disable=SC2086 # one keychain path per word
security list-keychains -d user -s "$kc" $existing

# Sign by certificate hash: the self-signed certificate isn't trusted, so it isn't
# listed as a "valid" identity, but codesign can still use it.
identity=$(security find-certificate -a -Z "$kc" | awk '/^SHA-1 hash:/ {print $3; exit}')
[[ -n $identity ]] || { echo "no certificate in the signing identity" >&2; exit 1; }
echo "signing with certificate $identity"

tar -xzf "$tarball" -C "$work"
app=$work/FreeCell.app
codesign --force --deep --timestamp=none --sign "$identity" --keychain "$kc" "$app"
codesign --verify --deep --strict --verbose=2 "$app"
codesign -d -r- "$app" 2>&1 | grep designated

COPYFILE_DISABLE=1 tar -czf "$tarball" -C "$work" FreeCell.app

dmgroot=$work/dmg
mkdir "$dmgroot"
ditto "$app" "$dmgroot/FreeCell.app"
ln -s /Applications "$dmgroot/Applications"
rm -f "$base.dmg"
hdiutil create -quiet -volname FreeCell -srcfolder "$dmgroot" -format UDZO "$base.dmg"
codesign --force --timestamp=none --sign "$identity" --keychain "$kc" "$base.dmg"
codesign --verify --verbose=2 "$base.dmg"
ls -la "$dir"
