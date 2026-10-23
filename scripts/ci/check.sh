#!/usr/bin/env bash
# Static checks that need no build: shell scripts, the desktop entry and the
# version. CI runs this on every pull request.
set -uo pipefail
cd "$(dirname "$0")/../.." || exit 1
failures=0
check() {
  local label=$1 out
  shift
  if ! out=$("$@" 2>&1); then
    echo "FAIL $label"
    printf '    %s\n' "${out//$'\n'/$'\n'    }"
    failures=$((failures + 1))
  else
    echo "ok   $label"
  fi
}
for f in $(git ls-files '*.sh'); do
  check "bash -n $f" bash -n "$f"
  check "shellcheck $f" shellcheck -x "$f"
done
for f in $(git ls-files '*.desktop'); do
  check "desktop-file-validate $f" desktop-file-validate "$f"
done
check "version" scripts/ci/version.sh
exit $((failures > 0))
