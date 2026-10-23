#!/usr/bin/env bash
# CI (release): write the update feed, then GPG-sign every release asset.
#   sign-release.sh <dir> <version>
# Needs FREECELL_GPG_KEY (armored private key, no passphrase). Writes:
#   latest.json        the feed the in-app update checker reads (version, and each
#                      platform's update file with its SHA-256)
#   <file>.sig         a detached signature for every file
#   SHA256SUMS(.sig)   checksums of every file, signed
#   freecell-signing-key.asc  the public key
set -euo pipefail
dir=$(realpath "${1:?asset dir}")
version=${2:?version}
: "${FREECELL_GPG_KEY:?FREECELL_GPG_KEY is not set}"
repo=${GITHUB_REPOSITORY:-jpsutton/freecell}
cd "$dir"
rm -f ./*.sig SHA256SUMS* latest.json freecell-signing-key.asc

# Update file per platform: what the update checker downloads.
assets='{}'
for pair in \
  "macos-arm64 FreeCell-$version-macos-arm64.app.tar.gz" \
  "linux-x86_64 FreeCell-$version-x86_64.AppImage" \
  "windows-x64 FreeCell-$version-windows-x64.zip"; do
  key=${pair%% *}
  file=${pair#* }
  if [[ ! -f $file ]]; then
    echo "::warning::$file not found; latest.json leaves out $key"
    continue
  fi
  assets=$(jq --arg k "$key" --arg name "$file" \
    --arg url "https://github.com/$repo/releases/download/v$version/$file" \
    --arg sha "$(sha256sum "$file" | cut -d' ' -f1)" \
    '. + {($k): {name: $name, url: $url, sha256: $sha}}' <<<"$assets")
done
jq -n --arg v "$version" --arg url "https://github.com/$repo/releases/tag/v$version" \
  --argjson a "$assets" '{version: $v, url: $url, assets: $a}' > latest.json
cat latest.json

export GNUPGHOME
GNUPGHOME=$(mktemp -d)
trap 'gpgconf --kill all; rm -rf "$GNUPGHOME"' EXIT
gpg --batch --quiet --import <<<"$FREECELL_GPG_KEY"
fpr=$(gpg --batch --with-colons --list-secret-keys | awk -F: '/^fpr/ {print $10; exit}')
echo "signing with $fpr"

for file in ./*; do
  [[ -f $file ]] || continue
  gpg --batch --yes --local-user "$fpr" --detach-sign --no-armor "$file"
done
gpg --batch --armor --export "$fpr" > freecell-signing-key.asc
sha256sum -- * | grep -v ' SHA256SUMS' > SHA256SUMS
gpg --batch --yes --local-user "$fpr" --detach-sign --no-armor SHA256SUMS

for sig in ./*.sig; do
  gpg --batch --quiet --verify "$sig" "${sig%.sig}"
done
ls -la
