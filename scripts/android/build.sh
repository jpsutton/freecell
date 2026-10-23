#!/usr/bin/env bash
# Local Android build in Docker: builds the toolchain image (once), then a
# debug-signed arm64 APK that installs with `adb install`.
#   scripts/android/build.sh [build-type]   (default: Release)
# Writes build/android/FreeCell-<version>-arm64-v8a.apk. Gradle's downloads
# are kept in build/android-cache between runs.
set -euo pipefail
cd "$(dirname "$0")/../.."
type=${1:-Release}
image=freecell-android:qt6.11.3

docker image inspect "$image" > /dev/null 2>&1 || docker build -t "$image" scripts/android

mkdir -p build/android build/android-cache
docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME=/cache/home -v "$PWD:/src" -v "$PWD/build/android-cache:/cache" "$image" bash -euc '
    mkdir -p "$HOME"
    "$QT_ANDROID/bin/qt-cmake" -S /src -B /src/build/android/tree -G Ninja \
      -DCMAKE_BUILD_TYPE='"$type"' -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
      -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" -DQT_ANDROID_ABIS=arm64-v8a
    cmake --build /src/build/android/tree --target apk
    apk=$(find /src/build/android/tree/android-build -name "*.apk" -path "*outputs*" | head -1)
    # A release APK comes out unsigned; sign it with a local debug key so it
    # installs for testing. Not for distribution.
    if [[ $apk == *unsigned* ]]; then
      ks=/cache/debug.keystore
      [[ -f $ks ]] || keytool -genkeypair -keystore "$ks" -storepass android -keypass android \
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" > /dev/null
      bt=$(ls -d "$ANDROID_SDK_ROOT"/build-tools/* | tail -1)
      "$bt/zipalign" -f -p 4 "$apk" /tmp/aligned.apk
      "$bt/apksigner" sign --ks "$ks" --ks-pass pass:android --out /tmp/signed.apk /tmp/aligned.apk
      apk=/tmp/signed.apk
    fi
    cp "$apk" /src/build/android/out.apk
  '
version=$(scripts/ci/version.sh)
mv build/android/out.apk "build/android/FreeCell-$version-arm64-v8a.apk"
echo "build/android/FreeCell-$version-arm64-v8a.apk"
