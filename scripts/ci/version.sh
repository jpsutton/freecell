#!/usr/bin/env bash
# Print the app version: project(... VERSION x.y.z) in CMakeLists.txt.
set -euo pipefail
cd "$(dirname "$0")/../.."
version=$(sed -n 's/^project(freecell VERSION \([0-9][0-9.]*\) .*/\1/p' CMakeLists.txt)
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "no x.y.z version in CMakeLists.txt" >&2; exit 1; }
echo "$version"
