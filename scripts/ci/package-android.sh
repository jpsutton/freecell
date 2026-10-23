#!/usr/bin/env bash
# CI: configure and build the Android APK (arm64-v8a), then write it to <out-dir>.
#   package-android.sh <build-dir> <out-dir>
# Needs QT_ANDROID (Qt for Android), QT_HOST (desktop Qt of the same version),
# ANDROID_SDK_ROOT and ANDROID_NDK_ROOT, and JAVA_HOME. Writes
# <out-dir>/FreeCell-<version>-arm64-v8a.apk. A release build comes out unsigned,
# so it is signed here with a throwaway debug key: that APK installs for testing
# but is not a release build. With UNSIGNED=1 the unsigned APK is written as it is,
# for release.yml to sign with the release key.
set -euo pipefail
build=$(realpath -m "${1:?build dir}")
out=$(realpath -m "${2:?out dir}")
: "${QT_ANDROID:?QT_ANDROID is not set}" "${QT_HOST:?QT_HOST is not set}"
: "${ANDROID_SDK_ROOT:?ANDROID_SDK_ROOT is not set}" "${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT is not set}"
cd "$(dirname "$0")/../.."
version=$(scripts/ci/version.sh)
mkdir -p "$out"

"$QT_ANDROID/bin/qt-cmake" -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
  -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" -DQT_ANDROID_ABIS=arm64-v8a \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build "$build" --target apk

apk=$(find "$build/android-build" -name '*.apk' -path '*outputs*' | head -1)
[[ -n $apk ]] || { echo "no APK was built" >&2; exit 1; }
if [[ $apk == *unsigned* && -z ${UNSIGNED:-} ]]; then
  ks=$build/debug.keystore
  rm -f "$ks"
  keytool -genkeypair -keystore "$ks" -storepass android -keypass android \
    -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US" > /dev/null
  bt=$(find "$ANDROID_SDK_ROOT/build-tools" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)
  "$bt/zipalign" -f -p 4 "$apk" "$build/aligned.apk"
  "$bt/apksigner" sign --ks "$ks" --ks-pass pass:android --out "$build/signed.apk" "$build/aligned.apk"
  apk=$build/signed.apk
fi
cp "$apk" "$out/FreeCell-$version-arm64-v8a.apk"
ls -la "$out"
