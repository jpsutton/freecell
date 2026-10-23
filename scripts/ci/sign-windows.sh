#!/usr/bin/env bash
# CI (release, Linux): Authenticode-sign FreeCell.exe inside the Windows zip.
#   sign-windows.sh <dir>
# <dir> holds FreeCell-<version>-windows-x64.zip from the build. Needs osslsigncode,
# FREECELL_CODESIGN_P12 (base64 PKCS#12) and FREECELL_CODESIGN_PASSWORD. The
# certificate is self-signed, so SmartScreen still warns; the signature names
# the publisher and shows the exe wasn't changed after the release was built.
set -euo pipefail
dir=$(realpath "${1:?asset dir}")
: "${FREECELL_CODESIGN_P12:?FREECELL_CODESIGN_P12 is not set}"
: "${FREECELL_CODESIGN_PASSWORD:?FREECELL_CODESIGN_PASSWORD is not set}"

zip=$(find "$dir" -maxdepth 1 -name 'FreeCell-*-windows-x64.zip' | head -n 1)
[[ -n $zip ]] || { echo "no Windows zip in $dir" >&2; exit 1; }
name=$(basename "$zip" .zip)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
umask 077
printf '%s' "$FREECELL_CODESIGN_P12" | base64 -d > "$work/identity.p12"
printf '%s' "$FREECELL_CODESIGN_PASSWORD" > "$work/pass"
# The PKCS#12 file uses the legacy (RC2/3DES) encryption that macOS's `security import`
# needs; OpenSSL 3 reads it only with -legacy. osslsigncode gets plain PEM files.
openssl pkcs12 -legacy -in "$work/identity.p12" -passin "file:$work/pass" -nokeys -clcerts \
  | openssl x509 -out "$work/cert.pem"
openssl pkcs12 -legacy -in "$work/identity.p12" -passin "file:$work/pass" -nocerts -nodes \
  | openssl pkey -out "$work/key.pem"

unzip -q "$zip" -d "$work"
exe=$work/$name/FreeCell.exe
sign() {
  osslsigncode sign -certs "$work/cert.pem" -key "$work/key.pem" -h sha256 \
    -n FreeCell -i https://github.com/jpsutton/freecell "$@" -in "$exe" -out "$exe.signed"
}
# A timestamp keeps the signature valid after the certificate expires. Sign without
# one if the timestamp server is down.
if ! sign -ts http://timestamp.digicert.com; then
  echo "::warning::timestamp server failed; signing without a timestamp"
  rm -f "$exe.signed"
  sign
fi
mv "$exe.signed" "$exe"
osslsigncode verify -CAfile "$work/cert.pem" -in "$exe"

rm -f "$zip"
(cd "$work" && zip -q -r -9 "$zip" "$name")
ls -la "$dir"
