#!/usr/bin/env bash
# Add a signed FreeCell APK to the F-Droid repository on the gh-pages branch and publish it.
# Keeps the newest three versions, and pushes gh-pages as
# one fresh commit each time, so the branch never grows past a few APKs.
#
#   scripts/ci/fdroid-repo.sh dist/FreeCell-<version>-arm64-v8a.apk
#
# Needs fdroidserver and the Android build tools, and in the environment:
#   FREECELL_FDROID_KEYSTORE       the repository key's keystore (PKCS12), base64
#   FREECELL_FDROID_KEYSTORE_PASS  its password
#   GITHUB_REPOSITORY              owner/name, for the Pages URL
#   FREECELL_PAGES_REMOTE          optional: where gh-pages lives (default: origin), for trying
#                                  this against a local repository
set -euo pipefail

apk=$(realpath "$1")
src=$(cd "$(dirname "$0")/../.." && pwd)
: "${FREECELL_FDROID_KEYSTORE:?}" "${FREECELL_FDROID_KEYSTORE_PASS:?}" "${GITHUB_REPOSITORY:?}"
owner=${GITHUB_REPOSITORY%%/*}
name=${GITHUB_REPOSITORY#*/}
url="https://${owner,,}.github.io/$name/fdroid/repo"
remote=${FREECELL_PAGES_REMOTE:-$(git -C "$src" remote get-url origin)}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
pages="$work/pages"
mkdir -p "$pages"
# Start from what is published, so the older versions stay listed.
if git -C "$src" fetch --depth 1 "$remote" gh-pages 2>/dev/null; then
    git -C "$src" archive FETCH_HEAD | tar -x -C "$pages"
fi
mkdir -p "$pages/fdroid/repo/icons" "$pages/fdroid/archive/icons" "$pages/fdroid/metadata"
cp "$src/packaging/android/fdroid/"*.yml "$pages/fdroid/metadata/"
# The repository's own icon, where fdroid looks for it.
for d in repo archive; do
    cp "$src/packaging/icons/freecell-128.png" "$pages/fdroid/$d/icons/icon.png"
done
cp "$apk" "$pages/fdroid/repo/"

# The repository key and fdroid's config live outside the published tree.
cfg="$work/cfg"
mkdir -p "$cfg"
printf '%s' "$FREECELL_FDROID_KEYSTORE" | base64 -d > "$cfg/keystore.p12"
cat > "$pages/fdroid/config.yml" <<EOF
repo_url: $url
repo_name: FreeCell
repo_description: >-
  FreeCell, a Windows XP FreeCell clone. Built and signed by the release workflow of
  https://github.com/$GITHUB_REPOSITORY.
archive_older: 3
repo_keyalias: freecell-fdroid
keystore: $cfg/keystore.p12
keystorepass: '$FREECELL_FDROID_KEYSTORE_PASS'
keypass: '$FREECELL_FDROID_KEYSTORE_PASS'
keydname: CN=FreeCell F-Droid repository
EOF
chmod 0600 "$pages/fdroid/config.yml" "$cfg/keystore.p12"

(cd "$pages/fdroid" && fdroid update --pretty --delete-unknown)
# Older versions move to the archive; this repository doesn't keep one.
rm -rf "$pages/fdroid/archive" "$pages/fdroid/config.yml" "$pages/fdroid/tmp" "$pages/fdroid/.fdroid."*

fingerprint=$(cd "$pages/fdroid" && keytool -list -v -keystore "$cfg/keystore.p12" -storetype PKCS12 \
    -storepass "$FREECELL_FDROID_KEYSTORE_PASS" -alias freecell-fdroid | sed -n 's/^\s*SHA256: //p' | tr -d ':')
cat > "$pages/fdroid/index.html" <<EOF
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FreeCell F-Droid repository</title>
<style>body{font:16px/1.5 system-ui,sans-serif;max-width:40rem;margin:3rem auto;padding:0 1rem}code{word-break:break-all}</style>
<h1>FreeCell F-Droid repository</h1>
<p>Add this repository to F-Droid, Droid-ify or Neo Store to install and update FreeCell.</p>
<p><a href="fdroidrepos://${owner,,}.github.io/$name/fdroid/repo?fingerprint=$fingerprint">Add to F-Droid</a> (on the phone)</p>
<p>Address: <code>$url</code></p>
<p>Fingerprint: <code>$fingerprint</code></p>
EOF

cd "$pages"
touch .nojekyll
git init -q -b gh-pages
# The checkout's credentials (actions/checkout keeps them in the repository's own config).
{ git -C "$src" config --get-regexp '^http\..*\.extraheader$' || true; } | while read -r key value; do
    git config "$key" "$value"
done
git add -A
git -c user.name="github-actions[bot]" -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
    commit -q -m "F-Droid repository: $(basename "$apk")"
git push -q -f "$remote" gh-pages
echo "F-Droid repository at $url (fingerprint $fingerprint)"
